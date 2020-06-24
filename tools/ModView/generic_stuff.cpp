// Filename:	generic_stuff.cpp
//

#include "stdafx.h"
#include "generic_stuff.h"

#include <QtWidgets/QMessageBox>
#include <algorithm>
#include <string>
#include "includes.h"
#include "StringUtils.h"


#define BASEDIRNAME			"base" 
#define BASEDIRNAME_XMEN	"development" 

// kept as non-hungarian labels purely because these 2 are so common in our projects...
//
char		qdir[1024]={0};			// "S:\"
char		gamedir[1024]={0};		// "S:\\baseq3\\"

// unlike "normal" SetQdirFromPath() functions, I need to see if qdir has changed 
//	(ie by loading an EF model over an SOF model) which could mean that we're in a different dir, and so
//	should lose all cached textures etc...
//
bool bXMenPathHack = false;
bool bQ3RulesApply = true;
static bool SetQdirFromPath2( const char *path, const char *psBaseDir );
void SetQdirFromPath( const char *path )
{
	char prevQdir[1024];
	strcpy (prevQdir, qdir);

	bXMenPathHack = false;	// MUST be here

	if (SetQdirFromPath2(path,BASEDIRNAME))
	{
		bQ3RulesApply = true;
	}
	else
	{
		if (SetQdirFromPath2(path,BASEDIRNAME_XMEN))
		{
			bQ3RulesApply = false;
			bXMenPathHack = true;
		}
	}		

	if (Q_stricmp(prevQdir,qdir))
	{
		extern void Media_Delete(void);
		Media_Delete();
	}
}

static bool SetQdirFromPath2( const char *path2, const char *psBaseDir )
{
//	static bool bDone = false;
	
//	if (!bDone)
	{
//		bDone = true;

		char	temp[1024];
		char    path[1024];
		strcpy(temp,path2);
		strcpy(path,temp);
		const char	*c;
		const char *sep;
		int		len, count;
		
//		if (!(path[0] == '/' || path[0] == '\\' || path[1] == ':'))
//		{	// path is partial
//			Q_getwd (temp);
//			strcat (temp, path);
//			path = temp;
//		}
		
		ToLower(path);
		
		// search for "base" in path from the RIGHT hand side (and must have a [back]slash just before it)
		
		len = strlen(psBaseDir);
		for (c=path+strlen(path)-1 ; c != path ; c--)
		{
			int i;
			
			if (!Q_stricmpn (c, psBaseDir, len)
				&& 
				(*(c-1) == '/' || *(c-1) == '\\')	// would be more efficient to do this first, but only checking after a strncasecmp ok ensures no invalid pointer-1 access
				)
			{			
				sep = c + len;
				count = 1;
				while (*sep && *sep != '/' && *sep != '\\')
				{
					sep++;
					count++;
				}
				strncpy (gamedir, path, c+len+count-path);
				gamedir[c+len+count-path]='\0';
				for ( i = 0; i < (int)strlen( gamedir ); i++ )
				{
					if ( gamedir[i] == '\\' ) 
						gamedir[i] = '/';
				}
	//			qprintf ("gamedir: %s\n", gamedir);

				strncpy (qdir, path, c-path);
				qdir[c-path] = 0;
				for ( i = 0; i < (int)strlen( qdir ); i++ )
				{
					if ( qdir[i] == '\\' ) 
						qdir[i] = '/';
				}
	//			qprintf ("qdir: %s\n", qdir);

				return true;
			}
		}
	//	Error ("SetQdirFromPath: no '%s' in %s", BASEDIRNAME, path);
	}

	return false;
}

// takes (eg) "q:\quake\baseq3\textures\borg\name.tga"
//
//	and produces "textures/borg/name.tga"
//
void Filename_RemoveQUAKEBASE( std::string& string )
{
	std::replace (string.begin(), string.end(), '\\', '/');
	ToLower (string);

/*
	int loc = string.Find("/quake");	
	if (loc >=0 )
	{
		loc = string.Find("/",loc+1);
		if (loc >=0)
		{
			// now pointing at "baseq3", "demoq3", whatever...
			loc = string.Find("/", loc+1);

			if (loc >= 0)
			{
				// now pointing at local filename...
				//
				string = string.Mid(loc+1);
			}
		}
	}	
*/

//	SetQdirFromPath( string );
	string = Replace (string, gamedir, "");
}









bool FileExists (const char * psFilename)
{
	FILE *handle = fopen(psFilename, "r");
	if (!handle)
	{
		return false;
	}
	fclose (handle);
	return true;
}

long FileLen( const char * psFilename)
{
	FILE *handle = fopen(psFilename, "r");
	if (!handle)
	{
		return -1;
	}

	long l = filesize(handle);

	fclose (handle);
	return l;
}




// returns actual filename only, no path
//
char *Filename_WithoutPath(const char * psFilename)
{
	static char sString[MAX_QPATH];
/*
	const char * p = strrchr(psFilename,'\\');

	if (!p++)
	{
		p = strrchr(psFilename,'/');
		if (!p++)
			p=psFilename;
	}

	strcpy(sString,p);
*/

	const char * psCopyPos = psFilename;
	
	while (*psFilename)
	{
		if (*psFilename == '/' || *psFilename == '\\')
			psCopyPos = psFilename+1;
		psFilename++;
	}

	strcpy(sString,psCopyPos);

	return sString;

}


// returns (eg) "\dir\name" for "\dir\name.bmp"
//
char *Filename_WithoutExt(const char * psFilename)
{
	static char sString[MAX_QPATH];

	strcpy(sString,psFilename);

	char *p = strrchr(sString,'.');		
	char *p2= strrchr(sString,'\\');
	char *p3= strrchr(sString,'/');

	// special check, make sure the first suffix we found from the end wasn't just a directory suffix (eg on a path'd filename with no extension anyway)
	//
	if (p && 
		(p2==0 || (p2 && p>p2)) &&
		(p3==0 || (p3 && p>p3))
		)
		*p=0;	

	return sString;

}


// loses anything after the path (if any), (eg) "\dir\name.bmp" becomes "\dir"
//
char *Filename_PathOnly(const char * psFilename)
{
	static char sString[MAX_QPATH];

	strcpy(sString,psFilename);	
	
//	for (int i=0; i<strlen(sString); i++)
//	{
//		if (sString[i] == '/')
//			sString[i] = '\\';
//	}
		
	char *p1= strrchr(sString,'\\');
	char *p2= strrchr(sString,'/');
	char *p = (p1>p2)?p1:p2;
	if (p)
		*p=0;

	return sString;

}


// returns filename's extension only (including '.'), else returns original string if no '.' in it...
//
char *Filename_ExtOnly(const char * psFilename)
{
	static char sString[MAX_QPATH];
	const char * p = strrchr(psFilename,'.');

	if (!p)
		p=psFilename;

	strcpy(sString,p);

	return sString;

}


// used when sending output to a text file etc, keeps columns nice and neat...
//
char *String_EnsureMinLength(const char * psString, int iMinLength)
{
	static char	sString[8][1024];
	static int i=0;

	i=(i+1)&7;

	strcpy(sString[i],psString);

	// a bit slow and lazy, but who cares?...
	//
	while (strlen(sString[i])<(unsigned int)iMinLength)
		strcat(sString[i]," ");

	return sString[i];

}


// similar to strlwr, but (crucially) doesn't touch original...
//
char *String_ToLower(const char * psString)
{
	static char sString[MAX_QPATH];

	strcpy(sString,psString);
	
	return ToLower(sString);	

}

// similar to strupr, but (crucially) doesn't touch original...
//
char *String_ToUpper(const char * psString)
{
	static char sString[MAX_QPATH];

	strcpy(sString,psString);
	return ToUpper(sString);

}

char *String_ForwardSlash(const char * psString)
{
	static char sString[MAX_QPATH];

	strcpy(sString,psString);

	char *p;
	while ( (p=strchr(sString,'\\')) != NULL)
	{
		*p = '/';
	}
	
	return ToLower(sString);
}

char	*va(const char *format, ...)
{
	va_list		argptr;
	static char		string[16][16384];		// important to have a good depth to this. 16 is nice
	static int index = 0;

	index = (++index)&15;
	
	va_start (argptr, format);
//	vsprintf (string[index], format,argptr);
	vsnprintf(string[index], sizeof(string[0]), format, argptr);
	va_end (argptr);
	string[index][sizeof(string[0])-1] = '\0';

	return string[index];	
}


#ifdef _WIN32
// returns a path to somewhere writeable, without trailing backslash...
//
// (for extra speed now, only evaluates it on the first call, change this if you like)
//
char *scGetTempPath(void)
{
	static char sBuffer[MAX_QPATH];
	unsigned int dwReturnedSize;
	static int i=0;
	
	if (!i++)
	{
		dwReturnedSize = GetTempPath(sizeof(sBuffer),sBuffer);
		
		if (dwReturnedSize>sizeof(sBuffer))
		{
			// temp path too long to return, so forget it...
			//
			strcpy(sBuffer,"c:");	// "c:\\");	// should be writeable
		}
		
		// strip any trailing backslash...
		//
		if (sBuffer[strlen(sBuffer)-1]=='\\')
			sBuffer[strlen(sBuffer)-1]='\0';
	}// if (!i++)
	
	return sBuffer;
	
}
#endif

// these MUST all be MB_TASKMODAL boxes now!!
//
bool gbErrorBox_Inhibit = false;	// Keith wanted to be able to remote-deactivate these...
std::string strLastError;
const char * ModView_GetLastError()		// function for him to be able to interrogate if he's disabled the error boxes
{
	return strLastError.c_str();
}


extern bool Gallery_Active();
extern void Gallery_AddError	(const char * psText);
extern void Gallery_AddInfo		(const char * psText);
extern void Gallery_AddWarning	(const char * psText);

void ErrorBox(const char *sString)
{
	/*if (Gallery_Active())
	{
		Gallery_AddError(sString);
		Gallery_AddError("\n");
		return;
	}*/

	if (!gbErrorBox_Inhibit)
	{
		QMessageBox::critical (NULL, "Error", sString);
	}

	strLastError = sString;
}
void InfoBox(const char *sString)
{
	/*if (Gallery_Active())
	{
		Gallery_AddInfo(sString);
		Gallery_AddInfo("\n");
		return;
	}*/

	QMessageBox::information (NULL, "Information", sString);
}
void WarningBox(const char *sString)
{
	/*if (Gallery_Active())
	{
		Gallery_AddWarning(sString);
		Gallery_AddWarning("\n");
		return;
	}*/

    QMessageBox::warning (NULL, "Warning", sString);
}



bool GetYesNo(const char *psQuery)
{
	/*if (Gallery_Active())
	{
		Gallery_AddWarning("GetYesNo call (... to which I guessed 'NO' ) using this query...\n\n");
		Gallery_AddWarning(psQuery);
		Gallery_AddWarning("\n");
		return false;
	}*/

	return QMessageBox::question (NULL, "Query", psQuery) == QMessageBox::Yes;
}

void StatusMessage(const char * psString)	// param can be NULL to indicate fallback to "ready" or whatever you want
{
	/*CMainFrame* pMainFrame = (CMainFrame*) AfxGetMainWnd();
	if (pMainFrame)
	{
		pMainFrame->StatusMessage(psString);
	}*/
}


/* this piece copied out of on-line examples, it basically works as a
	similar function to Ffilelength(int handle), but it works with streams */
long filesize(FILE *handle)
{
   long curpos, length;

   curpos = ftell(handle);
   fseek(handle, 0L, SEEK_END);
   length = ftell(handle);
   fseek(handle, curpos, SEEK_SET);

   return length;
}


// returns -1 for error
int LoadFile (const char * psPathedFilename, void **bufferptr, int bReportErrors)
{
	FILE	*f;
	int    length;
	char    *buffer;

	f = fopen(psPathedFilename,"rb");
	if (f)
	{
		length = filesize(f);	
		buffer = static_cast<char *>(malloc (length + 1));
		buffer[length] = 0;
		int lread = fread (buffer, 1, length, f);	
		fclose (f);

		if (lread==length)
		{	
			*bufferptr = buffer;
			return length;
		}
		free(buffer);
	}

	extern bool gbInImageLoader;	// tacky, but keeps quieter errors
	if (bReportErrors && !gbInImageLoader)
	{
		ErrorBox(va("Error reading file %s!",psPathedFilename));
	}
	return -1;
}


// returns -1 for error
int SaveFile (const char * psPathedFilename, const void *pBuffer, int iSize)
{		
#ifdef USE_MFC
	extern void CreatePath (const char *path);
	CreatePath(psPathedFilename);
	FILE *f = fopen(psPathedFilename,"wb");
	if (f)
	{
		int iLength = fwrite(pBuffer, 1, iSize, f);
		fclose (f);

		if (iLength == iSize)
		{	
			return iLength;
		}

		ErrorBox(va("Error writing file \"%s\" (length %d), only %d bytes written!",psPathedFilename,iSize,iLength));
		return -1;
	}

	ErrorBox(va("Error opening file \"%s\" for writing!",psPathedFilename));
	
	return -1;
#else
	return iSize;
#endif
}



// this'll return a string of up to the first 4095 chars of a system error message...
//
const char * SystemErrorString(unsigned int dwError)
{
	static char sString[4096];
#ifdef _WIN32
	LPVOID lpMsgBuf=0;

	FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		dwError,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR) &lpMsgBuf,
		0,
		NULL 
	);		

	ZEROMEM(sString);
	strncpy(sString,(const char *) lpMsgBuf,sizeof(sString)-1);

	LocalFree( lpMsgBuf ); 
#endif
	return sString;
}


void SystemErrorBox(unsigned int dwError)
{
	ErrorBox( SystemErrorString(dwError) );
}

bool SendFileToNotepad(const char * psFilename)
{
#ifdef _WIN32
	bool bReturn = false;

	char sExecString[MAX_QPATH];

	sprintf(sExecString,"notepad %s",psFilename);

	if (WinExec(sExecString,	//  const char * lpCmdLine,  // address of command line
				SW_SHOWNORMAL	//  UINT uCmdShow      // window style for new application
				)
		>31	// don't ask me, Windoze just uses >31 as OK in this call.
		)
	{
		// ok...
		//
		bReturn = true;
	}
	else
	{
		ErrorBox("Unable to locate/run NOTEPAD on this machine!\n\n(let me know about this -Ste)");		
	}

	return bReturn;
#else
	return true;
#endif
}

// creates as temp file, then spawns notepad with it...
//
bool SendStringToNotepad(const char * psWhatever, const char * psLocalFileName)
{
#ifdef _WIN32
	bool bReturn = false;

	const char * psOutputFileName = va("%s\\%s",scGetTempPath(),psLocalFileName);

	FILE *handle = fopen(psOutputFileName,"wt");
	if (handle)
	{
		fputs(psWhatever, handle);
		fclose(handle);

		bReturn = SendFileToNotepad(psOutputFileName);
	}
	else
	{
		ErrorBox(va("Unable to create file \"%s\" for notepad to use!",psOutputFileName));
	}

	return bReturn;
#else
	return true;
#endif
}



////////////////////////// eof ///////////////////////////