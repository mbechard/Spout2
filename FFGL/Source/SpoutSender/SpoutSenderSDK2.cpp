﻿/*

	FFGL plugin for sending DirectX texture to a Spout receiver
	With compatible hardware sends texture, otherwise memoryshare

	Original 01.06.13  - first version based on RR-DXGLBridge
	Copyright 2013 Elio <elio@r-revue.de>
	Note fix to FFGL.cpp to allow setting string parameters
	http://resolume.com/forum/viewtopic.php?f=14&t=10324

	Now rewritten with Spout SDK - Version 3

	See also SpoutReceiver.cpp
	
	----------------------------------------------------------------------------------
	24.06.14 - major revision using SpoutSDK - renamed project to SpoutSenderSDK2
	08-07-14 - Version 3.000
	14.07-14 - changed to fixed SpoutSender object
	16.07.14 - restored host fbo binding after writetexture
	25.07.14 - Version 3.001
			 - changed option to true DX9 mode rather than just compatible format
			 - recompiled with latest SDK
	16.08.14 - used DrawToSharedTexture
			 - Version 3.002
	18.08.14 - recompiled for testing and copied to GitHub
	20.08.14 - sender name existence check
			 - activated event locks
			 - Version 3.003
			 - recompiled for testing and copied to GitHub

*/
#include "SpoutSenderSDK2.h"
#include <FFGL.h>
#include <FFGLLib.h>

// To force memoryshare
// #define MemoryShareMode
#ifndef MemoryShareMode
	#define FFPARAM_SharingName (0)
	#define FFPARAM_Update      (1)
	#define FFPARAM_DX11        (2)
#endif


////////////////////////////////////////////////////////////////////////////////////////////////////
//  Plugin information
////////////////////////////////////////////////////////////////////////////////////////////////////
static CFFGLPluginInfo PluginInfo (

	SpoutSenderSDK2::CreateInstance,		// Create method
	#ifndef MemoryShareMode
	"OF46",									// Plugin unique ID - LJ note 4 chars only
	"SpoutSender2",							// Plugin name - LJ note 16 chars only ! see freeframe.h
	1,										// API major version number
	001,									// API minor version number
	2,										// Plugin major version number
	001,									// Plugin minor version number
	FF_EFFECT,								// Plugin type
	"Spout SDK DX11 sender",				// Plugin description - uses strdup
	#else
	"OF47",									// Plugin unique ID - LJ note 4 chars only
	"SpoutSender2M",						// Plugin name - LJ note 16 chars only ! see freeframe.h
	1,										// API major version number
	001,									// API minor version number
	2,										// Plugin major version number
	001,									// Plugin minor version number
	FF_EFFECT,								// Plugin type
	"Spout Memoryshare sender",				// Plugin description - uses strdup
	#endif
	"- - - - - - Vers 3.003 - - - - - -"	// About - uses strdup
);


////////////////////////////////////////////////////////////////////////////////////////////////////
//  Constructor and destructor
////////////////////////////////////////////////////////////////////////////////////////////////////
SpoutSenderSDK2::SpoutSenderSDK2() : CFreeFrameGLPlugin(), m_initResources(1), m_maxCoordsLocation(-1)
{
	// Input properties
	SetMinInputs(1);
	SetMaxInputs(1);


	/*
	// Debug console window so printf works
	FILE* pCout;
	AllocConsole();
	freopen_s(&pCout, "CONOUT$", "w", stdout); 
	printf("SpoutSender2 Vers 3.003\n");
	*/


	// initial values
	bMemoryMode       = false;
	bInitialized      = false;
	bDX9mode          = true; // DirectX 9 mode rather than DirectX 11
	m_Width           = 0;
	m_Height          = 0;
	UserSenderName[0] = 0;
	SenderName[0]     = 0;

	// Compilation memoryshare
	#ifdef MemoryShareMode
	bMemoryMode = true;
	#else
	bMemoryMode = false;
	#endif

	// Set parameters if not memoryshare mode
	#ifndef MemoryShareMode
	SetParamInfo(FFPARAM_SharingName,   "Sender Name",      FF_TYPE_TEXT,    "");
	SetParamInfo(FFPARAM_Update,        "Update",           FF_TYPE_EVENT,   false );
	SetParamInfo(FFPARAM_DX11,          "DirectX 11",       FF_TYPE_BOOLEAN, false );
	#endif

	// For memory mode, tell Spout to use memoryshare 
	// Default is false or detected according to compatibility
	if(bMemoryMode) {
		sender.SetMemoryShareMode(true);
		// Give it a user name for ProcessOpenGL
		strcpy_s(UserSenderName, 256, "MemoryShare"); 
	}

	// Set DirectX mode depending on DX9 flag
	if(bDX9mode) 
		sender.SetDX9(true);
	else 
	    sender.SetDX9(false);


}


SpoutSenderSDK2::~SpoutSenderSDK2()
{
	// ReleaseSender does nothing if there is no sender
	sender.ReleaseSender();

}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  Methods
////////////////////////////////////////////////////////////////////////////////////////////////////
DWORD SpoutSenderSDK2::InitGL(const FFGLViewportStruct *vp)
{
	// initialize FFGL gl extensions
	m_extensions.Initialize();

	return FF_SUCCESS;
}


DWORD SpoutSenderSDK2::DeInitGL()
{
	return FF_SUCCESS;
}


DWORD SpoutSenderSDK2::ProcessOpenGL(ProcessOpenGLStruct *pGL)
{
	
	// We need a texture to process
	if (pGL->numInputTextures < 1) return FF_FAIL;
	if (pGL->inputTextures[0] == NULL) return FF_FAIL;
  
	FFGLTextureStruct &InputTexture = *(pGL->inputTextures[0]);

	// get the max s,t that correspond to the width, height
	// of the used portion of the allocated texture space
	FFGLTexCoords maxCoords = GetMaxGLTexCoords(InputTexture);

	// Draw now whether a sender has initialized or not
	DrawTexture(InputTexture.Handle, maxCoords);

	// If there is no sender name yet, the sender cannot be created
	if(!UserSenderName[0]) {
		return FF_SUCCESS; // keep waiting for a name
	}
	// Otherwise create a sender if not initialized yet
	else if(!bInitialized) {

		// Update the sender name
		strcpy_s(SenderName, 256, UserSenderName); 

		// Set global width and height so any change can be tested
		m_Width  = (unsigned int)InputTexture.Width;
		m_Height = (unsigned int)InputTexture.Height;

		// Create a new sender
		bInitialized = sender.CreateSender(SenderName, m_Width, m_Height);
		if(!bInitialized) {
			char temp[256];
			sprintf(temp,"Could not create sender\n%s\nTry another name", SenderName);
			MessageBox(NULL, temp, "Spout", MB_OK);
			UserSenderName[0] = 0; // wait for another name to be entered
		}

		return FF_SUCCESS; // give it one frame to initialize
	}
	// Has the texture size or user entered sender name changed
	else if(m_Width  != (unsigned int)InputTexture.Width 
		 || m_Height != (unsigned int)InputTexture.Height
		 || strcmp(SenderName, UserSenderName) != 0 ) {

			// Release existing sender
			sender.ReleaseSender();
			bInitialized = false;
			return FF_SUCCESS; // return for initialization on the next frame
	}

	// Now we can send the FFGL texture
	SaveOpenGLstate();

	// Render the Freeframe texture into the shared texture
	sender.DrawToSharedTexture(InputTexture.Handle, GL_TEXTURE_2D,  m_Width, m_Height, (float)maxCoords.s, (float)maxCoords.t);

	RestoreOpenGLstate();

	// Important - Restore the FFGL host FBO binding because Spout uses a local fbo
	if(pGL->HostFBO) glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, pGL->HostFBO);

	return FF_SUCCESS;

}

DWORD SpoutSenderSDK2::GetParameter(DWORD dwIndex)
{
	DWORD dwRet = FF_FAIL;

	#ifndef MemoryShareMode
	switch (dwIndex) {
		case FFPARAM_SharingName:
			if(!bMemoryMode) dwRet = (DWORD)UserSenderName;
			return dwRet;
		default:
			return FF_FAIL;
	}
	#endif

	return FF_FAIL;
}


DWORD SpoutSenderSDK2::SetParameter(const SetParameterStruct* pParam)
{
	HGLRC glContext = wglGetCurrentContext();
	bool bDX9;

	// The parameters will not exist for memoryshare mode
	#ifndef MemoryShareMode
	if (pParam != NULL) {

		switch (pParam->ParameterNumber) {

			case FFPARAM_SharingName:
				if(pParam->NewParameterValue && strlen((char*)pParam->NewParameterValue) > 0) {
					strcpy_s(UserSenderName, (char*)pParam->NewParameterValue);
				}
				break;

			// Update user entered name
			case FFPARAM_Update :
				if (pParam->NewParameterValue) { 
					// Is it different to the current sender name ?
					if(strcmp(SenderName, UserSenderName) != 0) {
						// Create a new sender
						if(bInitialized) sender.ReleaseSender();
						// ProcessOpenGL will pick up the change
						bInitialized = false; 
					}
				}
				break;

			// Set DirectX 11 mode
			case FFPARAM_DX11 :
				if(pParam->NewParameterValue > 0)
					bDX9 = false;
				else
					bDX9 = true;

				// Any change ?
				if(bDX9 != bDX9mode) {
					bDX9mode = bDX9;
					sender.SetDX9(bDX9mode);
					// Create a new sender
					if(bInitialized) sender.ReleaseSender();
					bInitialized = false;
				}

				break;

			default:
				break;

		}
		return FF_SUCCESS;
	}
	#endif

	return FF_FAIL;
}


void SpoutSenderSDK2::DrawTexture(GLuint TextureHandle, FFGLTexCoords maxCoords)
{

	glColor4f(1.f, 1.f, 1.f, 1.f);

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, TextureHandle);

	glBegin(GL_QUADS);

	glTexCoord2f(0.0, 0.0);	
	glVertex2f(-1.0, -1.0);

	glTexCoord2f(0.0, (float)maxCoords.t);
	glVertex2f(-1.0, 1.0);

	glTexCoord2f((float)maxCoords.s, (float)maxCoords.t);
	glVertex2f(1.0, 1.0);
	
	glTexCoord2f((float)maxCoords.s, 0.0);
	glVertex2f(1.0, -1.0);

	glEnd();

	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_TEXTURE_2D);

}


void SpoutSenderSDK2::SaveOpenGLstate()
{

	// save texture state, client state, etc.
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);

	glMatrixMode(GL_TEXTURE);
	glPushMatrix();
	glLoadIdentity();

	glPushAttrib(GL_TRANSFORM_BIT);
	glPushAttrib(GL_TRANSFORM_BIT);
	glGetFloatv(GL_VIEWPORT, vpdim);
	glViewport(0, 0, m_Width, m_Height);
	
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity(); // reset the current matrix back to its default state
	glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0f, 1.0f);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
}


void SpoutSenderSDK2::RestoreOpenGLstate()
{
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
		
	glPopAttrib();
	glViewport((int)vpdim[0], (int)vpdim[1], (int)vpdim[2], (int)vpdim[3]);
		
	glMatrixMode(GL_TEXTURE);
	glPopMatrix();

	glPopClientAttrib();			
	glPopAttrib();

}
