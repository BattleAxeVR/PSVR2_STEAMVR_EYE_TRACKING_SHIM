# PSVR2_STEAMVR_EYE_TRACKING_SHIM

PSVR 2 Eye-Tracking via OpenXR EXT_gaze_interaction provided by SteamVR itself

NOTE: THIS REPO HAS NO NDA'ed 3RD PARTY PRIVATE UNRELEASED CODE or BINARIES IN IT.

WARNING: USE AT YOUR OWN RISK!!! No Warranty is provided!

This work relies on a THIRD PARTY closed-source and as-yet unreleased DLL (do NOT ask me for it).

Calibration isn't finished yet, it will be done in a separate app.

<img width="808" height="631" alt="image" src="https://github.com/user-attachments/assets/1c95fb33-5fdf-4cc7-b2d7-5a3df6539897" />


<img width="520" height="921" alt="image" src="https://github.com/user-attachments/assets/9f38f2d6-bf80-404a-a728-e0d5d20d9c8a" />

Driver shim method, framework and technique created and shared freely (MIT) by mbucchia here:

https://github.com/mbucchia/Pimax-EyeTracker-SteamVR


BUILD INSTRUCTIONS:

First, follow the instructions / README in the other project to get familiar with it. All credit is due to mbucchia here, I just plugged in the PSVR 2 gazes instead of Pimax, using IPC.

This client code or server (DLL) have to be compiled to use the same named pipe string for direct IPC communication of the gazes and handshake,

#define PSVR2_SERVER_NAMED_PIPE_NAME "\\.\pipe\PlaystationVR2ServerPipe"

I also reduced the data sent over IPC to avoid transferring junk / garbage.

	struct XRGazeState
	{
		XrVector3f direction_ = { 0.0f, 0.0f, -1.0f };
		bool is_valid_ = false;
	};

	struct AllXRGazeStates
	{
		XRGazeState combined_gaze_;
		XRGazeState per_eye_gazes_[BVR::NUM_EYES];
	};

AllXRGazeStates is the data type that should be sent over IPC. I also did not use a thread to copy the data out from IPC on the client, it is fast enough I think to do it synchronously as I have.

NOTE: EXT gaze interaction only uses combined_gaze_ above, not per_eye_gazes_, but xrLocate could, in theory, switch to either open eye (via 1/2 IPD) and the client could then figure out which eye is open and which is shut. EXT could also always return the gaze from your dominant eye, for ex.

Note 2: Use the provided OpenXR_EXT_Gaze_Interaction_Tester.exe (in the root dir and in Releases) to verify your installation is working. If not, check OpenXR Explorer showing "supportsEyeGazeTnteraction is true". Just use Search feature to find this tool.

<img width="651" height="113" alt="image" src="https://github.com/user-attachments/assets/409c42e2-aa5b-472d-bdee-65a3df21d4b8" />

