/*
UWSetPositionOrientationFromUDP.cpp

This plugin allows the position (lat, lon, alt) and the orientation (phi, theta, psi) of the aircraft to be set by reading in the values from a UDP socket.

*/

#include "XPLMDisplay.h"
#include "XPLMGraphics.h"
//#include "XPLMProcessing.h"
#include "XPLMDataAccess.h"
//#include "XPLMMenus.h"
#include "XPLMUtilities.h"
//#include "XPWidgets.h"
//#include "XPStandardWidgets.h"
//#include "XPLMCamera.h"
//#include <string.h>
#include <stdio.h>
//#include <stdlib.h>

#include "PracticalSocket.h"   // For UDPSocket and SocketException


#if IBM
#include <windows.h>
#endif
	

//----------------------------GLOBAL VARIALBES----------------------------------------
#define MAX_ITEMS 11
#define UDP_PORT_RECEIVE 49003		//port to listen to to receive UDP packets

const int MAXRCVSTRING = 4096; // Longest string to receive
XPLMWindowID	gWindow = NULL;			//for displaying plugin status
XPLMHotKeyID	gHotKey = NULL;
XPLMDataRef		gPositionDataRef[MAX_ITEMS];
int				gClicked = 0;

//Notes about the DataRefs
//	sim/flightmodel/position/local_x		double	y	meters	The location of the plane in OpenGL coordinates
//	sim/flightmodel/position/local_y		double	y	meters	The location of the plane in OpenGL coordinates
//	sim/flightmodel/position/local_z		double	y	meters	The location of the plane in OpenGL coordinates
//	sim/flightmodel/position/lat_ref		float	n	degrees	The latitude of the point 0,0,0 in OpenGL coordinates (Writing NOT recommended!!)
//	sim/flightmodel/position/lon_ref		float	n	degrees	The longitude of the point 0,0,0 in OpenGL coordinates.
//	sim/flightmodel/position/theta			float	y	degrees	The pitch relative to the plane normal to the Y axis in degrees
//	sim/flightmodel/position/phi			float	y	degrees	The roll of the aircraft in degrees
//	sim/flightmodel/position/psi			float	y	degrees	The true heading of the aircraft in degrees from the Z axis
//	sim/flightmodel/position/latitude		double	n	degrees	The latitude of the aircraft
//	sim/flightmodel/position/longitude		double	n	degrees	The longitude of the aircraft
//	sim/flightmodel/position/elevation		double	n	meters	The elevation above MSL of the aircraft
char DataRefString[MAX_ITEMS][255] = {	"sim/flightmodel/position/local_x", "sim/flightmodel/position/local_y", "sim/flightmodel/position/local_z",
										"sim/flightmodel/position/lat_ref", "sim/flightmodel/position/lon_ref",	"sim/flightmodel/position/theta",
										"sim/flightmodel/position/phi", "sim/flightmodel/position/psi",
										"sim/flightmodel/position/latitude", "sim/flightmodel/position/longitude", "sim/flightmodel/position/elevation"};




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



//-------------------IMPLEMENT THE X-PLANE PLUGIN INTERFACE---------------------------
PLUGIN_API int XPluginStart(
						char *		outName,
						char *		outSig,
						char *		outDesc)
{
	strcpy(outName, "UWSetPositionOrientationFromUDP");
	strcpy(outSig, "xpsdk.examples.UWSetPositionOrientationFromUDP");
	strcpy(outDesc, "A plug-in that sets the position/orientation of the vehicle to a fixed value from a UDP socket.  Be sure that X-Plane Physics Engine is disabled before using (see UWDiablePhysicsEngine).");

	//fill up the gPositionDataRef array with data references
	for (int Item=0; Item<MAX_ITEMS; Item++)
		gPositionDataRef[Item] = XPLMFindDataRef(DataRefString[Item]);

	/* Now we create a window.  We pass in a rectangle in left, top,
	* right, bottom screen coordinates.  We pass in three callbacks. */
	int topLeftX = 725 - 2*350;
	int topLeftY = 440;
	int width = 250;
	int height = 250;
	gWindow = XPLMCreateWindow(
		topLeftX, topLeftY, topLeftX+width, topLeftY-height,			/* Area of the window. */
		1,							/* Start visible. */
		MyDrawWindowCallback,		/* Callbacks */
		MyHandleKeyCallback,
		MyHandleMouseClickCallback,
		NULL);						/* Refcon - not used. */

	/* Register our hot key for applying a new position. */
	gHotKey = XPLMRegisterHotKey(XPLM_VK_F6, xplm_DownFlag, 
		"Set Position and Orientation From File",
		MyHotKeyCallback,
		NULL);

	return 1;
}



PLUGIN_API void	XPluginStop(void)
{
	XPLMUnregisterHotKey(gHotKey);
}



PLUGIN_API void XPluginDisable(void)
{
}



PLUGIN_API int XPluginEnable(void)
{
	return 1;
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
		(char*)(gClicked ? "You are clicking here" : "UWSetPositionOrientationFromUDP (press F6)"), NULL, xplmFont_Basic);
	
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

		XPLMDrawString(color, left + 5, top - (i+2)*verticalLineSpacing, totalString, NULL, xplmFont_Basic);
	}
} 



/*
Read in info from a UDP socket and set the aircraft orientation and position to that value.

Note that this will block until it receives the UDP packet
*/
void	MyHotKeyCallback(void *               inRefcon)
{	
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

		char recvString[MAXRCVSTRING + 1]; // Buffer for echo string + \0
		string sourceAddress;              // Address of datagram source
		unsigned short sourcePort;         // Port of datagram source

		int bytesRcvd = sock.recvFrom(recvString, MAXRCVSTRING, sourceAddress, sourcePort);
		recvString[bytesRcvd] = '\0';  // Terminate string

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