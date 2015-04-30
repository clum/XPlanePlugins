/*
UWTimeProcessingUDP.cpp

This plugin allows the position (lat, lon, alt) and the orientation (phi, theta, psi) of the aircraft to be set by reading continuously reading values from a UDP socket.

*/


#if APL
#if defined(__MACH__)
#include <Carbon/Carbon.h>
#endif
#endif

#include <stdio.h>
#include <string.h>
#include "XPLMProcessing.h"
#include "XPLMDataAccess.h"
#include "XPLMUtilities.h"
#include "XPLMGraphics.h"
#include "XPLMDisplay.h"

#include "PracticalSocket.h"   // For UDPSocket and SocketException

//----------------------------GLOBAL VARIALBES----------------------------------------
#define MAX_ITEMS 11
#define UDP_PORT_RECEIVE 49003					//port to listen to to receive UDP packets
#define UDP_RECEIVE_DELTA_T 0.05				//time between UDP processes
const int MAXRCVSTRING = 4096;					// Longest string to receive

XPLMWindowID	gWindow = NULL;					//for displaying plugin status
XPLMHotKeyID	gHotKey = NULL;					//used to register our hotkey
XPLMDataRef		gPositionDataRef[MAX_ITEMS];	//hold all of the datarefs
int				gClicked = 0;					//used to determine if user is clicking in the window or not
bool			gListeningForUDPPackets;		//set this to true to start listening for UDP packets (it should start as false to avoid making X-Plane hang if no packets are incoming)
bool			gDisplayOverlay = false;		//set this to true to display the overlay on the X-Plane window which shows plugin information (useful for debugging).

char DataRefString[MAX_ITEMS][255] = {
	"sim/flightmodel/position/local_x", 
	"sim/flightmodel/position/local_y", 
	"sim/flightmodel/position/local_z",
	"sim/flightmodel/position/lat_ref", 
	"sim/flightmodel/position/lon_ref",	
	"sim/flightmodel/position/theta", 
	"sim/flightmodel/position/phi", 
	"sim/flightmodel/position/psi", 
	"sim/flightmodel/position/latitude", 
	"sim/flightmodel/position/longitude", 
	"sim/flightmodel/position/elevation"
};




//----------------------------FUNCTION PROTOTYPES-------------------------------------
void ApplyThetaPhiPsiToDataRefs(float theta, float phi, float psi);
void ApplyLatLonAltToDataRefs(double lat, double lon, double altitude);

void MyDrawWindowCallback(
                                   XPLMWindowID         inWindowID,    
                                   void *               inRefcon);    


void MyHandleKeyCallback(
                                   XPLMWindowID         inWindowID,    
                                   char                 inKey,    
                                   XPLMKeyFlags         inFlags,    
                                   char                 inVirtualKey,    
                                   void *               inRefcon,    
                                   int                  losingFocus);    

int MyHandleMouseClickCallback(
                                   XPLMWindowID         inWindowID,    
                                   int                  x,    
                                   int                  y,    
                                   XPLMMouseStatus      inMouse,    
                                   void *               inRefcon);   


void	MyHotKeyCallback(void *               inRefcon);    

float	MyFlightLoopCallback(
                                   float                inElapsedSinceLastCall,    
                                   float                inElapsedTimeSinceLastFlightLoop,    
                                   int                  inCounter,    
                                   void *               inRefcon);    



//-------------------IMPLEMENT THE X-PLANE PLUGIN INTERFACE---------------------------
PLUGIN_API int XPluginStart(
						char *		outName,
						char *		outSig,
						char *		outDesc)
{
	char	outputPath[255];
	#if APL && __MACH__
	char outputPath2[255];
	int Result = 0;
	#endif
		
	strcpy(outName, "UWTimedProcessingUDP");
	strcpy(outSig, "xplanesdk.examples.UWTimedProcessingUDP");
	strcpy(outDesc, "A plugin that listens to UDP packets and position and orientation based on this.");

	//Start up with listening for packets turned off (otherwise, x-plane will hang waiting for packets which may not be coming yet)
	gListeningForUDPPackets = false;

	//fill up the gPositionDataRef array with data references
	for (int Item=0; Item<MAX_ITEMS; Item++) {
		gPositionDataRef[Item] = XPLMFindDataRef(DataRefString[Item]);
	}
	
	if(gDisplayOverlay) {
		/* Now we create a window.  We pass in a rectangle in left, top,
		* right, bottom screen coordinates.  We pass in three callbacks. */
		int topLeftX = 725 - 2*350;
		int topLeftY = 440 - 225;

		int width = 250;
		int height = 250;
		gWindow = XPLMCreateWindow(
			topLeftX, topLeftY, topLeftX+width, topLeftY-height,			/* Area of the window. */
			1,							/* Start visible. */
			MyDrawWindowCallback,		/* Callbacks */
			MyHandleKeyCallback,
			MyHandleMouseClickCallback,
			NULL);						/* Refcon - not used. */
	}

	/* Register our hot key for applying a new position. */
	gHotKey = XPLMRegisterHotKey(XPLM_VK_F5, xplm_DownFlag, 
		"Toggle listening/stop listening for UDP packets",
		MyHotKeyCallback,
		NULL);


	/* Register our callback for once a second.  Positive intervals
	 * are in seconds, negative are the negative of sim frames.  Zero
	 * registers but does not schedule a callback for time. */
	XPLMRegisterFlightLoopCallback(		
			MyFlightLoopCallback,	/* Callback */
			UDP_RECEIVE_DELTA_T,					/* Interval */
			NULL);					/* refcon not used. */
			
	return 1;
}



PLUGIN_API void	XPluginStop(void)
{
	/* Unregister the callback */
	XPLMUnregisterFlightLoopCallback(MyFlightLoopCallback, NULL);
	
	///* Close the file */
	//fclose(gOutputFile);
}



PLUGIN_API void XPluginDisable(void)
{
	///* Flush the file when we are disabled.  This is convenient; you 
	// * can disable the plugin and then look at the output on disk. */
	//fflush(gOutputFile);
}



PLUGIN_API int XPluginEnable(void)
{
	return 1;
}



PLUGIN_API void XPluginReceiveMessage(
					XPLMPluginID	inFromWho,
					long			inMessage,
					void *			inParam)
{
}



//-------------------------FUNCTION DEFINITIONS---------------------------------------
/*
Apply the specified Euler angles to the datarefs
*/
void ApplyThetaPhiPsiToDataRefs(float theta, float phi, float psi)
{
	//apply the angles to the datarefs
	XPLMSetDataf(gPositionDataRef[5], theta);
	XPLMSetDataf(gPositionDataRef[6], phi);
	XPLMSetDataf(gPositionDataRef[7], psi);
}



/*
Apply the specified lat/lon/alt to the datarefs.  Note the units on the inputs arguments 

latitude in degrees
longitude in degrees
altitude in meters

This will then use XPLMWorldToLocal to also compute the local_x, local_y, local_z and apply them to the datarefs
*/
void ApplyLatLonAltToDataRefs(double latitudeDeg, double longitudeDeg, double altitudeMeters)
{
	//apply the positions to the data refs
	XPLMSetDatad(gPositionDataRef[8], latitudeDeg);
	XPLMSetDatad(gPositionDataRef[9], longitudeDeg);
	XPLMSetDatad(gPositionDataRef[10], altitudeMeters);

	//compute the local_x, local_y, and local_z values
	double local_x;
	double local_y;
	double local_z;
	XPLMWorldToLocal(latitudeDeg, longitudeDeg, altitudeMeters, &local_x, &local_y, &local_z);

	//apply the local_x, local_y, and local_z values to the datarefs
	XPLMSetDatad(gPositionDataRef[0], local_x);
	XPLMSetDatad(gPositionDataRef[1], local_y);
	XPLMSetDatad(gPositionDataRef[2], local_z);
}




/*
Process what to do at timed intervals
*/
float	MyFlightLoopCallback(
                                   float                inElapsedSinceLastCall,    
                                   float                inElapsedTimeSinceLastFlightLoop,    
                                   int                  inCounter,    
                                   void *               inRefcon)
{
	/* The actual callback.  First we read the sim's time and the data. */
	float	elapsed = XPLMGetElapsedTime();

	if(gListeningForUDPPackets) {
		//Setup the containers for values we want to get from UDP
		double phiDegDouble;
		double thetaDegDouble;
		double psiDegDouble;
		double latitudeDeg;
		double longitudeDeg;
		double altitudeMeters;

		float thetaDeg;
		float phiDeg;
		float psiDeg;

		//receive the data from the UDP port
		try {
			unsigned short echoServPort = UDP_PORT_RECEIVE;
			UDPSocket sock(echoServPort);                

			char recvString[MAXRCVSTRING + 1]; // This will be the buffer for echo string + \0
			string sourceAddress;              // Address of datagram source
			unsigned short sourcePort;         // Port of datagram source
			int bytesRcvd = sock.recvFrom(recvString, MAXRCVSTRING, sourceAddress, sourcePort);
			recvString[bytesRcvd] = '\0';		// Terminate string

			//Convert the recieved string to values
			char *pch;			
			pch = strtok(recvString," ");

			for(int wordNumber = 0; wordNumber < 6; wordNumber++)
			{
				double value = atof(pch);

				if (wordNumber==0) {
					phiDegDouble = value;

				} else if (wordNumber==1) {
					thetaDegDouble = value;

				} else if (wordNumber==2) {
					psiDegDouble = value;

				} else if (wordNumber==3) {
					latitudeDeg = value;

				} else if (wordNumber==4) {
					longitudeDeg = value;

				} else if (wordNumber==5) {
					altitudeMeters = value;

				} else {
				}		

				pch = strtok (NULL, " ");
			}

			//convert phi, theta, psi to floats
			phiDeg		= (float)phiDegDouble;
			thetaDeg	= (float)thetaDegDouble;
			psiDeg		= (float)psiDegDouble;

		} catch (SocketException &e) {

		}

		//Set these values to the datarefs
		ApplyThetaPhiPsiToDataRefs(thetaDeg, phiDeg, psiDeg);
		ApplyLatLonAltToDataRefs(latitudeDeg, longitudeDeg, altitudeMeters);
	}

	/* Return UDP_RECEIVE_DELTA_T to indicate that we want to be called again in UDP_RECEIVE_DELTA_T second. */
	//DOES THIS NEED TO BE CHANGED TO CALLING RATE?
	return UDP_RECEIVE_DELTA_T;
}                                   



/*
 * MyDrawingWindowCallback
 * 
 * This callback does the work of drawing our window once per sim cycle each time
 * it is needed.  It dynamically changes the text depending on the saved mouse
 * status.  Note that we don't have to tell X-Plane to redraw us when our text
 * changes; we are redrawn by the sim continuously.
 * 
 */
void MyDrawWindowCallback(
                                   XPLMWindowID         inWindowID,    
                                   void *               inRefcon)
{
	int		left, top, right, bottom;
	float	color[] = { 1.0, 1.0, 1.0 }; 	/* RGB White */
	int		verticalLineSpacing = 10;
	
	/* First we get the location of the window passed in to us. */
	XPLMGetWindowGeometry(inWindowID, &left, &top, &right, &bottom);
	
	/* We now use an XPLMGraphics routine to draw a translucent dark
	 * rectangle that is our window's shape. */
	XPLMDrawTranslucentDarkBox(left, top, right, bottom);

	/* Finally we draw the text into the window, also using XPLMGraphics
	 * routines.  The NULL indicates no word wrapping. */
		
	//Line 1 (display plugin info)
	XPLMDrawString(color, left + 5, top - 1*verticalLineSpacing, 
		(char*)(gClicked ? "You are clicking here" : "UWTimedProcessingUDP"), NULL, xplmFont_Basic);
	
	//Line 2 (plugin instructions)
	XPLMDrawString(color, left + 5, top - 2*verticalLineSpacing, "Press F5 to toggle UDP listening on/off", NULL, xplmFont_Basic);
	
	//Line 3 (display status of listening or not)
	if(gListeningForUDPPackets) {
		XPLMDrawString(color, left + 5, top - 3*verticalLineSpacing, "Currently listening for UDP packets", NULL, xplmFont_Basic);
	} else {
		XPLMDrawString(color, left + 5, top - 3*verticalLineSpacing, "Currently not listening for UDP packets", NULL, xplmFont_Basic);
	}

	////Line 4 & 5 (display port number that UDP is listening for)
	char UDP_port_receive_char[10];
	itoa(UDP_PORT_RECEIVE, UDP_port_receive_char, 10);
	
	XPLMDrawString(color, left + 5, top - 4*verticalLineSpacing, "Port number for UDP packets displayed below", NULL, xplmFont_Basic);
	XPLMDrawString(color, left + 5, top - 5*verticalLineSpacing, UDP_port_receive_char, NULL, xplmFont_Basic);
	
	//Print out all the data refs
	for(int i = 0; i < MAX_ITEMS; i++) 
	{
		char currentValue_char[20];
		if((i==0) || (i==1) || (i==2) || (i==8) || (i==9) || (i==10)) {
			//doubles stored at these locations
			double valueDouble = XPLMGetDatad(gPositionDataRef[i]);
			sprintf(currentValue_char, "%f", valueDouble);

		} else {
			//floats stored at these locations
			float valueFloat = XPLMGetDataf(gPositionDataRef[i]);
			sprintf(currentValue_char, "%f", valueFloat);
		}
		
		char totalString[300];
		strcpy(totalString, DataRefString[i]);
		strcat(totalString, " is ");
		strcat(totalString, currentValue_char);

		XPLMDrawString(color, left + 5, top - (i+6)*verticalLineSpacing, totalString, NULL, xplmFont_Basic);
	}
} 



/*
Read in info from a UDP socket and set the aircraft orientation and position to that value.

Note that this will block until it receives the UDP packet
*/
void	MyHotKeyCallback(void *               inRefcon)
{	
	//toggle the gListeningForUDPPackets
	if(gListeningForUDPPackets) {
		gListeningForUDPPackets = false;
	} else {
		gListeningForUDPPackets = true;
	}
}



/*
 * MyHandleKeyCallback
 * 
 * Our key handling callback does nothing in this plugin.  This is ok; 
 * we simply don't use keyboard input.
 * 
 */
void MyHandleKeyCallback(
                                   XPLMWindowID         inWindowID,    
                                   char                 inKey,    
                                   XPLMKeyFlags         inFlags,    
                                   char                 inVirtualKey,    
                                   void *               inRefcon,    
                                   int                  losingFocus)
{
}   



/*
 * MyHandleMouseClickCallback
 * 
 * Our mouse click callback toggles the status of our mouse variable 
 * as the mouse is clicked.  We then update our text on the next sim 
 * cycle.
 * 
 */
int MyHandleMouseClickCallback(
                                   XPLMWindowID         inWindowID,    
                                   int                  x,    
                                   int                  y,    
                                   XPLMMouseStatus      inMouse,    
                                   void *               inRefcon)
{
	/* If we get a down or up, toggle our status click.  We will
	 * never get a down without an up if we accept the down. */
	if ((inMouse == xplm_MouseDown) || (inMouse == xplm_MouseUp))
		gClicked = 1 - gClicked;
	
	/* Returning 1 tells X-Plane that we 'accepted' the click; otherwise
	 * it would be passed to the next window behind us.  If we accept
	 * the click we get mouse moved and mouse up callbacks, if we don't
	 * we do not get any more callbacks.  It is worth noting that we 
	 * will receive mouse moved and mouse up even if the mouse is dragged
	 * out of our window's box as long as the click started in our window's 
	 * box. */
	return 1;
}    