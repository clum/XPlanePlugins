/*
 * UWPlugin.c
 * 
 * This plugin disables the X-Plane internal physics engine and makes the simulation suitable to position the aircraft with new positions and orientations.

	 To use this plugin, press the F9 key to toggle between disable/enbale the physics engine.

 * 
 */

#include "XPLMDataAccess.h"
#include "XPLMDisplay.h"
#include "XPLMGraphics.h"


//----------------------------GLOBAL VARIALBES----------------------------------------
#define NUM_VEHICLES 20		//number of vehicles in the sim/operation/override/override_planepath array

XPLMHotKeyID	gHotKey = NULL;
XPLMDataRef		gOverRidePlanePosition = NULL;
XPLMWindowID	gWindow = NULL;					
int				gClicked = 0;
bool			gPhysicsEngineDisabled;		//set this to true to disable the physics engine (it should start as false so X-Plane will not appear frozen (disabled) at startup)
bool			gDisplayOverlay = false;	//set this to true to display the overlay on the X-Plane window which shows plugin information (useful for debugging).

//----------------------------FUNCTION PROTOTYPES-------------------------------------
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
	strcpy(outName, "UWDisablePhysicsEngine");
	strcpy(outSig, "xplanesdk.examples.UWDisablePhysicsEngine");
	strcpy(outDesc, "A plugin that disables X-Planes internal physics engine (F9 to disable physics engine).");

	//Start up with physics engine enabled (otherwise, x-plane will appear frozen since the physics is turned off)
	gPhysicsEngineDisabled = false;

	/* Prefetch the sim variables we will use. */
	gOverRidePlanePosition = XPLMFindDataRef("sim/operation/override/override_planepath");
	
	if(gDisplayOverlay) {

		/* Now we create a window.  We pass in a rectangle in left, top,
		* right, bottom screen coordinates.  We pass in three callbacks. */
		int topLeftX = 725;
		int topLeftY = 500;
		int width = 250;
		int height = 50;
		gWindow = XPLMCreateWindow(
			topLeftX, topLeftY, topLeftX+width, topLeftY-height,			/* Area of the window. */
			1,							/* Start visible. */
			MyDrawWindowCallback,		/* Callbacks */
			MyHandleKeyCallback,
			MyHandleMouseClickCallback,
			NULL);						/* Refcon - not used. */
	}

	/* Register our hot key for the new view. */
	gHotKey = XPLMRegisterHotKey(XPLM_VK_F9, xplm_DownFlag, 
				"Disable Physics Engine",
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



PLUGIN_API void XPluginReceiveMessage(
					XPLMPluginID	inFromWho,
					long			inMessage,
					void *			inParam)
{
}




//-------------------------FUNCTION DEFINITIONS---------------------------------------
/*
Disable/enable the physics engine
*/
void	MyHotKeyCallback(void *               inRefcon)
{
	//user pressed the hot-key, take approriate action
	int intVals[NUM_VEHICLES];
	memset(intVals, 0, sizeof(intVals));

	if(gPhysicsEngineDisabled) {
		//re-enable the physics engine
		intVals[0] = 0;		//only disable your own aircraft
		gPhysicsEngineDisabled = false;

	} else {
		//Disable the physics engine
		intVals[0] = 1;		//only disable your own aircraft
		gPhysicsEngineDisabled = true;
	}

	int inOffset = 0;
	int inCount = NUM_VEHICLES;
	XPLMSetDatavi(gOverRidePlanePosition, intVals, inOffset, inCount);
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
		(char*)(gClicked ? "You are clicking here" : "UWDiablePhysicsEngine"), NULL, xplmFont_Basic);
	
	//Line 2 (Instruction on how to operate plugin)
	XPLMDrawString(color, left + 5, top - 2*verticalLineSpacing, "F9 to toggle physics engine on/off", NULL, xplmFont_Basic);

	////Line 2 (did the data reference get set ok?)
	//if(gOverRidePlanePosition==NULL) {
	//	XPLMDrawString(color, left + 5, top - 2*verticalLineSpacing, 
	//	"gOverRidePlanePosition is NULL", NULL, xplmFont_Basic);
	//
	//} else {
	//	XPLMDrawString(color, left + 5, top - 2*verticalLineSpacing, 
	//	"gOverRidePlanePosition is OK", NULL, xplmFont_Basic);
	//
	//}
	
	//Line 3 & 4 (get the status of the physics engine and display it to the window)
	int outValues[NUM_VEHICLES];
	int inOffset = 0;
	int inMax = NUM_VEHICLES;
	int arraySize = XPLMGetDatavi(gOverRidePlanePosition, outValues, inOffset, inMax);
	int overRidePlanePosition = outValues[0];
	char overRidePlanePosition_char[10];
	itoa(overRidePlanePosition, overRidePlanePosition_char, 10);
	
	XPLMDrawString(color, left + 5, top - 3*verticalLineSpacing, "Physics engine disabled? (0=no, 1=yes)", NULL, xplmFont_Basic);
	XPLMDrawString(color, left + 5, top - 4*verticalLineSpacing, overRidePlanePosition_char, NULL, xplmFont_Basic);

} 