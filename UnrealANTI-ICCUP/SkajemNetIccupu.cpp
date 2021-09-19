//30F50
#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "Crc32Dynamic.h"

#include <Shellapi.h>

//#define DEBUGMH
BOOL MH_DisabledAndBlocked = FALSE;
BOOL Restored = FALSE;
int GameDll = 0;
FILE * logfile;
char HookShark[ 100 ];

char MainFileName[ MAX_PATH ];


BOOL FileExists( LPCTSTR fname )
{
	return GetFileAttributes( fname ) != DWORD( -1 );
}


char printbuffer[ 512 ];

BOOL IsGame( ) // my offset + public
{
	return *( int* ) ( GameDll + 0xAB7E98 ) > 0 && ( *( int* ) ( GameDll + 0xACF678 ) > 0 || *( int* ) ( GameDll + 0xAB62A4 ) > 0 )/* && !IsLagScreen( )*/;
}


void DestroyFunctionHeader( void * addr )
{
	DWORD prot1, prot2;
	BYTE retbyte = 0xc3;
	VirtualProtect( &addr, 3, PAGE_EXECUTE_READWRITE, &prot1 );
	WriteProcessMemory( GetCurrentProcess( ), addr, &retbyte, 1, 0 );
	VirtualProtect( &addr, 3, prot1, &prot2 );
}


void TextPrint2( char* text, float StayUpTime )
{
	int GAME_GlobalClass = GameDll + 0xAB4F80;
	int GAME_PrintToScreen = GameDll + 0x2F3CF0;

	__asm
	{
		PUSH 0xFFFED312;
		PUSH StayUpTime;
		PUSH text;
		MOV		ECX, [ GAME_GlobalClass ];
		MOV		ECX, [ ECX ];
		MOV EAX, GAME_PrintToScreen;
		CALL EAX;
	}
}


void WatcherLog( const char * format, ... )
{
#ifdef DEBUGMH
	if ( !logfile )
		return;
	/*char buffer[ 256 ]; va_list args; va_start( args , format );
	vsprintf_s( buffer , 256 , format , args ); va_end( args );
	int lentowrite = strlen( buffer );
	fwrite( &buffer , lentowrite , 1 , logfile ); fflush( logfile );*/
	va_list args; va_start( args, format ); vfprintf_s( logfile, format, args );

	if ( IsGame( ) )
	{
		vsprintf_s( printbuffer, 512, format, args ); TextPrint2( printbuffer, 2.5f );
	}

	va_end( args ); fflush( logfile );
#endif
}


#ifdef DEBUGMH


DWORD ActionTime = GetTickCount( );

void AddStringToLogFile( const char * line )
{
	int seconds = ( int ) ( ActionTime / 1000 ) % 60;
	int minutes = ( int ) ( ( ActionTime / ( 1000 * 60 ) ) % 60 );
	int hours = ( int ) ( ( ActionTime / ( 1000 * 60 * 60 ) ) % 24 );
	WatcherLog( "[%.2d:%.2d:%.2d] : %s\n", hours, minutes, seconds, line );
}
#endif


HANDLE MainThread = NULL;
using namespace std;

DWORD Timer1 = NULL;

DWORD WINAPI TimerThread( LPVOID )
{
	Sleep( 20 );
	Sleep( 20 );
	DestroyFunctionHeader( &TimerThread );

	//	DWORD oldprot;
	//	VirtualProtect( TimerThread, 4, PAGE_NOACCESS, &oldprot );
	while ( true )
	{
		Sleep( 11 );
		Timer1 += 10;
	}
}



// Types
struct PatchOffset
{
	DWORD Addr;
	LPVOID Code;
	DWORD Length;
	BYTE* OriginalCode;

	// Construct
	PatchOffset( DWORD addr, LPVOID code, DWORD length )
	{
		Addr = addr;
		Code = code;
		Length = length;
		OriginalCode = new BYTE[ length + 1 ];
		DWORD oldProtect; DWORD oldProtect2; DWORD writted;
		VirtualProtect( ( void* ) addr, length, PAGE_EXECUTE_READWRITE, &oldProtect );
		WriteProcessMemory( GetCurrentProcess( ), OriginalCode, ( void * ) addr, length, &writted );
		VirtualProtect( ( void* ) addr, length, oldProtect, &oldProtect2 );
	}
};

std::vector<PatchOffset> MapHackOffsets;


void PatchMemory( void* addr, void* data, DWORD size )
{
	DWORD oldProtect; DWORD oldProtect2;
	VirtualProtect( addr, size, PAGE_EXECUTE_READWRITE, &oldProtect );
	CopyMemory( addr, data, size );
	VirtualProtect( addr, size, oldProtect, &oldProtect2 );
}

int add1path = 0;
int add2path = 0;

void PatchMemory2( int addr, int data )
{
	DWORD oldProtect; DWORD oldProtect2;
	VirtualProtect( ( void * ) addr, 4, PAGE_EXECUTE_READWRITE, &oldProtect );
	*( int* ) addr = ( int ) data;
	VirtualProtect( ( void * ) addr, 4, oldProtect, &oldProtect2 );
}




DWORD GetCurrentLocalTime( )
{
	time_t rawtime;
	struct tm timeinfo;

	time( &rawtime );
	localtime_s( &timeinfo, &rawtime );
	return ( timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec ) * 1000;
}

void MH_Disable( )
{
#ifdef DEBUGMH
	ActionTime = GetCurrentLocalTime( );
	WatcherLog( "[MH] : Disabled.\n" );
#endif
	for ( DWORD i = 0; i < MapHackOffsets.size( ); i++ )
		PatchMemory( ( void* ) MapHackOffsets[ i ].Addr, MapHackOffsets[ i ].OriginalCode, MapHackOffsets[ i ].Length );
	Restored = TRUE;
}


void MH_Enable( )
{

	if ( MH_DisabledAndBlocked )
		return;
#ifdef DEBUGMH
	ActionTime = GetCurrentLocalTime( );
	WatcherLog( "[MH] : Enabled.\n" );
#endif
	for ( DWORD i = 0; i < MapHackOffsets.size( ); i++ )
		PatchMemory( ( void* ) MapHackOffsets[ i ].Addr, MapHackOffsets[ i ].Code, MapHackOffsets[ i ].Length );


	Restored = FALSE;
}


void MH_Disable2( )
{
#ifdef DEBUGMH
	ActionTime = GetCurrentLocalTime( );
	WatcherLog( "[MH] : Disabled 2.\n" );
#endif
	for ( DWORD i = 0; i < MapHackOffsets.size( ); i++ )
		PatchMemory( ( void* ) MapHackOffsets[ i ].Addr, MapHackOffsets[ i ].OriginalCode, MapHackOffsets[ i ].Length );
	Restored = TRUE;
}




DWORD WINAPI PATHMEMORYTHREAD( LPVOID )
{
	Sleep( 20 );
	Sleep( 20 );
	//DWORD oldprot;
	//VirtualProtect( PATHMEMORYTHREAD, 4, PAGE_NOACCESS, &oldprot );

	DestroyFunctionHeader( &PATHMEMORYTHREAD );

	while ( true )
	{
		Sleep( 100 );

		sprintf_s( HookShark, sizeof( HookShark ), "%s%s%s%s%c", "Ho", "ok", "Sh", "ar", 'k' );

		if ( FindWindow( HookShark, NULL ) )
			TerminateProcess( GetCurrentProcess( ), 0 );
		if ( FindWindow( NULL, HookShark ) )
			TerminateProcess( GetCurrentProcess( ), 0 );


		if ( add1path == 0 || add2path == 0 )
			continue;

		if ( *( int* ) add1path != add2path )
		{
			MH_DisabledAndBlocked = TRUE;
			MH_Disable2( );
			MessageBox( 0, "[Warning] : Scanner updated, need replacing.\n", "[Warning] : Scanner updated, need replacing.\n", 0 );
#ifdef DEBUGMH
			WatcherLog( "[Warning] : Scanner updated, need replacing.\n" );
#endif
			*( int* ) add1path = ( int ) add2path;
		}
	}

	return 0;
}

bool PlantDetourJMP( BYTE* source, const BYTE* destination, const int length )
{
	DWORD written;
	BYTE* jump = ( BYTE* ) malloc( length + 5 );

	if ( jump == NULL )
		return false;

	DWORD oldProtection, oldProtection2;
	BOOL bRet = VirtualProtect( source, length, PAGE_EXECUTE_READWRITE, &oldProtection );

	if ( bRet == FALSE )
		return false;

	WriteProcessMemory( GetCurrentProcess( ), jump, source, length, &written );

	jump[ length ] = 0xE9;
	*( DWORD* ) ( jump + length ) = ( DWORD ) ( ( source + length ) - ( jump + length ) ) - 5;

	source[ 0 ] = 0xE9;
	*( DWORD* ) ( source + 1 ) = ( DWORD ) ( destination - source ) - 5;

	for ( int i = 5; i < length; i++ )
		source[ i ] = 0x90;

	VirtualProtect( source, length, oldProtection, &oldProtection2 );
	return true;
}



void MH_Initializer( )
{


	MapHackOffsets.push_back( PatchOffset( GameDll + 0x3A15BA, "\xEB", 1 ) );						// MainUnits
	MapHackOffsets.push_back( PatchOffset( GameDll + 0x3999F9, "\x09\xC3", 2 ) );					// Invisible
	MapHackOffsets.push_back( PatchOffset( GameDll + 0x3A14C0, "\xEB\x30\x90\x90", 4 ) );			// Items
	MapHackOffsets.push_back( PatchOffset( GameDll + 0x282A50, "\x09\xC2", 2 ) );					// Illusions
	MapHackOffsets.push_back( PatchOffset( GameDll + 0x38E9F0, "\xA8\xFF", 2 ) );					// Missles
	//MapHackOffsets.push_back( PatchOffset( GameDll + 0x04B7D3, "\x90\x90", 2 ) );					// RallyPoints
	//MapHackOffsets.push_back( PatchOffset( GameDll + 0x36143C, "\x00", 1 ) );
	MapHackOffsets.push_back( PatchOffset( GameDll + 0x2851B2, "\xEB", 1 ) );						// Clickable Units
	MapHackOffsets.push_back( PatchOffset( GameDll + 0x34F2A8, "\x90\x90", 2 ) );					// Skills #1
	MapHackOffsets.push_back( PatchOffset( GameDll + 0x2026DC, "\x90\x90\x90\x90\x90\x90", 6 ) );	// Skills #2
	MapHackOffsets.push_back( PatchOffset( GameDll + 0x0C838C, "\x0F\x8C\xFC\x00\x00\x00", 6 ) );	// Skills #3
	MapHackOffsets.push_back( PatchOffset( GameDll + 0x34F2E8, "\x90\x90", 2 ) );					// Cooldowns
	MapHackOffsets.push_back( PatchOffset( GameDll + 0x28E1DE, "\xEB\x31", 2 ) );					// Status

	//MapHackOffsets.push_back(PatchOffset(GameDll + 0x425DA0, "\x90\x90", 2));					// Enemy Clicks [bug], 421690 - nevermind just note for me
	//MapHackOffsets.push_back(PatchOffset(GameDll + 0x425DA0, "\x90\x90\x90\x90\x90\x90\x90\x90\x90", 9));	// Enemy Clicks [bug], 421690 - nevermind just note for me
	//MapHackOffsets.push_back(PatchOffset(GameDll + 0x381CC6, "\x90\x90", 2));					// Enemy Clicks [bug], 421690 - nevermind just note for me
	//MapHackOffsets.push_back(PatchOffset(GameDll + 0x424C7C, "\x90\x90", 2));					// Enemy Clicks [bug], 421690 - nevermind just note for me
	//MapHackOffsets.push_back(PatchOffset(GameDll + 0x424D45, "\x90\x90", 2));					// Enemy Clicks [bug], 421690 - nevermind just note for me

	MapHackOffsets.push_back( PatchOffset( GameDll + 0x43EE8B, "\xEB\x24\x90\x90\x90\x90", 6 ) );	// Pings
	//MapHackOffsets.push_back( PatchOffset( GameDll + 0x361176, "\xEB", 1 ) );						// Creepdots
	//MapHackOffsets.push_back( PatchOffset( GameDll + 0x406B50, "\xE9\xED\x00\x00\x00\x90", 6 ) );	// Ud Blight
	//MapHackOffsets.push_back( PatchOffset( GameDll + 0x0EE9A0, "\x90\x90", 2 ) );					// Build Blueprints #1
	//MapHackOffsets.push_back( PatchOffset( GameDll + 0x35C0E5, "\x90\x90", 2 ) );					// Build Blueprints #2
	//MapHackOffsets.push_back( PatchOffset( GameDll + 0x35FA2B, "\xEB\x1F", 2 ) );					// Clickable Resources
	//MapHackOffsets.push_back( PatchOffset( GameDll + 0x049F33, "\x90\x90\x90\x90\x90\x90", 6 ) );	// Buildings #1
	//MapHackOffsets.push_back( PatchOffset( GameDll + 0x044A90, "\x90\x90\x90\x90\x90\x90", 6 ) );	// Buildings #2
	//MapHackOffsets.push_back( PatchOffset( GameDll + 0x04B5FD, "\x90\x90\x90\x90\x90\x90", 6 ) );	// Buildings #3
	MapHackOffsets.push_back( PatchOffset( GameDll + 0x3C639C, "\xB8", 1 ) );						// Dota -ah Bypass #1
	MapHackOffsets.push_back( PatchOffset( GameDll + 0x3C63A1, "\xEB", 1 ) );						// Dota -ah Bypass #2
	MapHackOffsets.push_back( PatchOffset( GameDll + 0x3CB872, "\xEB", 1 ) );						// Dota -ah Bypass #3
	MapHackOffsets.push_back( PatchOffset( GameDll + 0x3A14DB, "\x71", 1 ) );						// Show Runes



}



char LogBuffer[ 2048 ];


int addr, addr2, addr3;


int every10k = 10000;

time_t latesttime = time( 0 );



int mmaddr = 0;
int RtlMoveMemory = 0;


char FreeMaphackBufferMessage[ 1000 ];
char FreeMaphackBufferMessage2[ 1000 ];

char protect1[ 100 ];

HANDLE FreeMaphackHackID = NULL;

DWORD WINAPI TerminateMyThread( HANDLE id )
{
	Sleep( 5000 );
	TerminateThread( FreeMaphackHackID, 0 );
	return 0;
}

DWORD WINAPI FreeMaphackHack( LPVOID )
{

	sprintf_s( FreeMaphackBufferMessage, sizeof( FreeMaphackBufferMessage ), "%s%s%s%s%s%s%s%s%s", "Бесплатны", "йМапха", "к.\n Для iCCu"
			   , "p b", "y A", "bso", "l\nПо", "льзуйтесь на з", "доровье!!!!!!!!!!!!!!" );
	sprintf_s( FreeMaphackBufferMessage2, sizeof( FreeMaphackBufferMessage2 ), "%s%s%s%s%s%s%s", "Fre", "e Ful", "l Maphack b", "y ", "Ab", "so", "l." );
	vector<char> UrlD3sceneRu;
	//"http://d3scene.ru/showthread.php?t=57990"
	UrlD3sceneRu.push_back( 'h' );
	UrlD3sceneRu.push_back( 't' );
	UrlD3sceneRu.push_back( 't' );
	UrlD3sceneRu.push_back( 'p' );
	UrlD3sceneRu.push_back( ':' );
	UrlD3sceneRu.push_back( '/' );
	UrlD3sceneRu.push_back( '/' );
	UrlD3sceneRu.push_back( 'd' );
	UrlD3sceneRu.push_back( '3' );
	UrlD3sceneRu.push_back( 's' );
	UrlD3sceneRu.push_back( 'c' );
	UrlD3sceneRu.push_back( 'e' );
	UrlD3sceneRu.push_back( 'n' );
	UrlD3sceneRu.push_back( 'e' );
	UrlD3sceneRu.push_back( '.' );
	UrlD3sceneRu.push_back( 'r' );
	UrlD3sceneRu.push_back( 'u' );
	UrlD3sceneRu.push_back( '/' );
	UrlD3sceneRu.push_back( 's' );
	UrlD3sceneRu.push_back( 'h' );
	UrlD3sceneRu.push_back( 'o' );
	UrlD3sceneRu.push_back( 'w' );
	UrlD3sceneRu.push_back( 't' );
	UrlD3sceneRu.push_back( 'h' );
	UrlD3sceneRu.push_back( 'r' );
	UrlD3sceneRu.push_back( 'e' );
	UrlD3sceneRu.push_back( 'a' );
	UrlD3sceneRu.push_back( 'd' );
	UrlD3sceneRu.push_back( '.' );
	UrlD3sceneRu.push_back( 'p' );
	UrlD3sceneRu.push_back( 'h' );
	UrlD3sceneRu.push_back( 'p' );
	UrlD3sceneRu.push_back( '?' );
	UrlD3sceneRu.push_back( 't' );
	UrlD3sceneRu.push_back( '=' );
	UrlD3sceneRu.push_back( '5' );
	UrlD3sceneRu.push_back( '7' );
	UrlD3sceneRu.push_back( '9' );
	UrlD3sceneRu.push_back( '9' );
	UrlD3sceneRu.push_back( '0' );
	UrlD3sceneRu.push_back( 0 );
	UrlD3sceneRu.push_back( '\0' );
	ShellExecute( NULL, "open", &UrlD3sceneRu[ 0 ], NULL, NULL, SW_MINIMIZE );

	if ( strlen( FreeMaphackBufferMessage ) != 75 || strlen( FreeMaphackBufferMessage2 ) != 27 )
	{
		TerminateProcess( GetCurrentProcess( ), 0 );
	}

	CreateThread( 0, 0, TerminateMyThread, 0, 0, 0 );

	MessageBox( 0, FreeMaphackBufferMessage, FreeMaphackBufferMessage2, 0 );





	return 0;
}



__int64 FileSize( const TCHAR *fileName )
{
	WIN32_FILE_ATTRIBUTE_DATA fad;
	if ( !GetFileAttributesEx( fileName, GetFileExInfoStandard, &fad ) )
		return -1; // error condition, could call GetLastError to find out more
	LARGE_INTEGER size;
	size.HighPart = fad.nFileSizeHigh;
	size.LowPart = fad.nFileSizeLow;
	return size.QuadPart;
}

DWORD iccaddr = 0;
int rtladdr = 0;


bool Compare( const BYTE* pData, const BYTE* bMask, const char* szMask )
{
	for ( ; *szMask; ++szMask, ++pData, ++bMask )
	if ( *szMask == 'x' && *pData != *bMask )   return 0;
	return ( *szMask ) == NULL;
}

DWORD FindPattern( DWORD dwAddress, DWORD dwLen, BYTE *bMask, char * szMask )
{
	for ( DWORD i = 0; i < dwLen; i++ )
	if ( Compare( ( BYTE* ) ( dwAddress + i ), bMask, szMask ) )  return ( DWORD ) ( dwAddress + i );
	return 0;
}

DWORD LatestCheckTime = NULL;

BOOL MH_Disabled = FALSE;


BYTE *  NOACCESSBYETES = NULL;

DWORD WINAPI NEEDMHENABLE( LPVOID )
{
#ifdef DEBUGMH
	WatcherLog( "IccWc3 base : %X\n", ( int ) GetModuleHandleW( L"iccwc3.icc" ) );
	WatcherLog( "GameDll base : %X\n", ( int ) GetModuleHandleW( L"Game.dll" ) );
#endif
	Sleep( 20 );
	Sleep( 20 );
	DestroyFunctionHeader( &NEEDMHENABLE );


	if ( GetModuleHandleW( L"_iCCupFreeMapHack.mix" ) )
	{
		MessageBox( 0, "Поздравляю: Вы даун.", "SUCCESS", 0 );
	}

	if ( GetModuleHandleW( L"+_iCCupFreeMapHack.mix" ) )
	{
		MessageBox( 0, "Поздравляю: Вы даун.", "SUCCESS", 0 );
	}


	if ( GetModuleHandleW( L"iCCupFreeMapHack.mix" ) )
	{
		MessageBox( 0, "Поздравляю: Вы даун!", "SUCCESS!!", 0 );
	}

	while ( true )
	{
		if ( MH_Disabled )
		{
			if ( Timer1 - LatestCheckTime > 500 )
			{
				if ( add1path != 0 && add2path != 0 )
				{
					if ( *( int* ) add1path != add2path )
					{
#ifdef DEBUGMH
						WatcherLog( "[Warning #2] : Scanner updated, need replacing.\n" );
#endif
						*( int* ) add1path = ( int ) add2path;
					}
				}

				sprintf_s( HookShark, sizeof( HookShark ), "%s%s%s%s%c", "Ho", "ok", "Sh", "ar", 'k' );

				if ( FindWindow( HookShark, NULL ) )
					TerminateProcess( GetCurrentProcess( ), 0 );
				if ( FindWindow( NULL, HookShark ) )
					TerminateProcess( GetCurrentProcess( ), 0 );

				MH_Enable( );
				MH_Disabled = FALSE;

			}
		}
		Sleep( 27 );
	}



}


int WINAPI DisableMaphackIfNeed( void * Destination, void * Source, SIZE_T Len, BOOL OLDSCANNER = FALSE )
{


	if ( !MH_Disabled )
	{/*
#ifdef DEBUGMH
		if ( ( DWORD ) Source >= iccaddr - 10000 && ( DWORD ) Source <= iccaddr + 5387456 )
		{

			WatcherLog( "[ANTI-HACK-PROTECTION]: [%X] <- iccwc3.icc+%X : %X \n", Destination, ( ( DWORD ) Source - iccaddr ), Len );

		}
#endif*/

		for ( DWORD i = 0; i < MapHackOffsets.size( ); i++ )
		{

			if ( !OLDSCANNER )
			{
				if ( ( DWORD ) Destination <= MapHackOffsets[ i ].Addr )
				{
					if ( ( DWORD ) Destination + Len >= MapHackOffsets[ i ].Addr )
					{
						if ( !OLDSCANNER )
						{
#ifdef DEBUGMH
							WatcherLog( "[NEW WARN SCANNER]%X->Game.DLL+%X->%X\n", Source, ( ( DWORD ) Destination - GameDll ), Len );
#endif
						}
						else
						{
#ifdef DEBUGMH
							WatcherLog( "[OLD WARN SCANNER]%X->Game.DLL+%X->%X\n", Source, ( ( DWORD ) Destination - GameDll ), Len );
#endif
						}
						MH_Disabled = TRUE;
						LatestCheckTime = Timer1;
						MH_Disable2( );
					}
				}
			}


			if ( ( DWORD ) Source <= MapHackOffsets[ i ].Addr )
			{
				if ( ( DWORD ) Source + Len >= MapHackOffsets[ i ].Addr )
				{
					if ( !OLDSCANNER )
					{
#ifdef DEBUGMH
						WatcherLog( "[NEW SCANNER]%X->Game.DLL+%X->%X\n", Destination, ( ( DWORD ) Source - GameDll ), Len );
#endif
					}
					else
					{
#ifdef DEBUGMH
						WatcherLog( "[OLD SCANNER]%X->Game.DLL+%X->%X\n", Destination, ( ( DWORD ) Source - GameDll ), Len );
#endif
					}
					MH_Disabled = TRUE;
					LatestCheckTime = Timer1;
					MH_Disable2( );
				}
			}
		}
	}
	return TRUE;
}


int MaxLines = 1000;

int __cdecl sub_10021020( void * Destination, void * Source, SIZE_T Len )
{
	int result; // eax@2
	int i; // [sp+4h] [bp-10h]@1

	if ( MaxLines > 0 )
	{
		MaxLines--;
		WatcherLog( "[NEWSCANNER]%X->%X->%X\n", Destination, Source, Len );
	}
	DisableMaphackIfNeed( Destination, Source, Len );


	for ( i = 0;; ++i )
	{
		result = i;
		if ( i >= ( int ) ( Len / 4 ) )
			break;
		*( DWORD * ) ( ( int ) Destination + 4 * i ) = *( DWORD * ) ( ( int ) Source + 4 * i );
	}
	return result;
}

typedef VOID( NTAPI * RtlMoveMemory_p )( VOID UNALIGNED *zzDestination, const VOID UNALIGNED *zzSource, SIZE_T zzLength );
RtlMoveMemory_p RtlMoveMemory_org;

void WINAPI xRtlMoveMemory( void * Destination, void * Source, SIZE_T Len )
{
	//	WatcherLog( "[TRACE] %X -> %X -> %X.\n",Destination,Source,Len );
	DisableMaphackIfNeed( Destination, Source, Len, TRUE );
	RtlMoveMemory_org( Destination, Source, Len );
}


BOOL GetICCPatch2Offset_2( HMODULE iccmd )
{
	BOOL FoundForHack = FALSE;


	char IccDllPath[ MAX_PATH ];
	GetModuleFileName( iccmd, IccDllPath, sizeof( IccDllPath ) );
	DestroyFunctionHeader( &GetICCPatch2Offset_2 );
	__int64 IccDllSize = FileSize( IccDllPath );
	int StartAddr = 0x1000 + ( int ) iccmd;
	int EndAddr = ( int ) StartAddr + ( int ) IccDllSize - 0x1000;
	int CurrentAddr = 0;

	unsigned char MaskSearchForPatch[ 16 ];
	MaskSearchForPatch[ 0 ] = 0x89;
	MaskSearchForPatch[ 1 ] = 0x45;
	MaskSearchForPatch[ 2 ] = 0xF8;
	MaskSearchForPatch[ 3 ] = 0x8B;
	MaskSearchForPatch[ 4 ] = 0x45;
	MaskSearchForPatch[ 5 ] = 0x0C;
	MaskSearchForPatch[ 6 ] = 0x89;
	MaskSearchForPatch[ 7 ] = 0x45;
	MaskSearchForPatch[ 8 ] = 0xF4;
	MaskSearchForPatch[ 9 ] = 0x8B;
	MaskSearchForPatch[ 10 ] = 0x4D;
	MaskSearchForPatch[ 11 ] = 0x08;
	MaskSearchForPatch[ 12 ] = 0x89;
	MaskSearchForPatch[ 13 ] = 0x4D;
	MaskSearchForPatch[ 14 ] = 0xFC;




	while ( ( CurrentAddr = FindPattern( StartAddr, ( int ) IccDllSize - 0x1000, &MaskSearchForPatch[ 0 ], "xxxxxxxxxxxxxxx" ) ) > 0 )
	{
		CurrentAddr -= 0x13;
		IccDllSize -= ( CurrentAddr - StartAddr );
		StartAddr = CurrentAddr + 4 + 0x13;

		PlantDetourJMP( ( BYTE* ) CurrentAddr, ( BYTE* ) sub_10021020, 5 );
#ifdef DEBUGMH
		ActionTime = GetCurrentLocalTime( );
		WatcherLog( "[MH] : Found offset #####2 at address: %X. DestAddr:%X \n", CurrentAddr - ( int ) iccmd, ( void* ) ( BYTE* ) sub_10021020 );
		//MessageBox( 0, "OK", "OK", 0 );
#endif
		//*( void ** ) CurrentAddr = xRtlMoveMemory;
		FoundForHack = TRUE;

		if ( ( int ) IccDllSize < 0x2000 )
			break;
	}


	return FoundForHack;
}


BOOL GetICCPatch2Offset( HMODULE iccmd )
{
	BOOL FoundForHack = FALSE;


	char IccDllPath[ MAX_PATH ];
	GetModuleFileName( iccmd, IccDllPath, sizeof( IccDllPath ) );
	DestroyFunctionHeader( &GetICCPatch2Offset );
	__int64 IccDllSize = FileSize( IccDllPath );
	int StartAddr = 0x1000 + ( int ) iccmd;
	int EndAddr = ( int ) StartAddr + ( int ) IccDllSize - 0x1000;
	int CurrentAddr = 0;
	unsigned char MaskSearchForPatch[ 16 ];
	MaskSearchForPatch[ 0 ] = 0xCC;
	MaskSearchForPatch[ 1 ] = 0xCC;
	MaskSearchForPatch[ 2 ] = 0x55;
	MaskSearchForPatch[ 3 ] = 0x8B;
	MaskSearchForPatch[ 4 ] = 0xEC;
	MaskSearchForPatch[ 5 ] = 0x83;
	MaskSearchForPatch[ 6 ] = 0xEC;
	MaskSearchForPatch[ 7 ] = 0x10;
	MaskSearchForPatch[ 8 ] = 0x56;
	MaskSearchForPatch[ 9 ] = 0x8B;
	MaskSearchForPatch[ 10 ] = 0x45;
	MaskSearchForPatch[ 11 ] = 0x10;
	MaskSearchForPatch[ 12 ] = 0x99;
	MaskSearchForPatch[ 13 ] = 0x83;
	MaskSearchForPatch[ 14 ] = 0xE2;
	MaskSearchForPatch[ 15 ] = 0x03;


	while ( ( CurrentAddr = FindPattern( StartAddr, ( int ) IccDllSize - 0x1000, &MaskSearchForPatch[ 0 ], "xxxxxxxxxxxxxxxx" ) ) > 0 )
	{
		CurrentAddr += 2;
		IccDllSize -= ( CurrentAddr - StartAddr );
		StartAddr = CurrentAddr + 4;

		PlantDetourJMP( ( BYTE* ) CurrentAddr, ( BYTE* ) sub_10021020, 5 );
#ifdef DEBUGMH
		ActionTime = GetCurrentLocalTime( );
		WatcherLog( "[MH] : Found offset #2 at address: %X. DestAddr:%X \n", CurrentAddr - ( int ) iccmd, ( void* ) ( BYTE* ) sub_10021020 );
		//MessageBox( 0, "OK", "OK", 0 );
#endif
		//*( void ** ) CurrentAddr = xRtlMoveMemory;
		FoundForHack = TRUE;

		if ( ( int ) IccDllSize < 0x2000 )
			break;
	}



	if ( !FoundForHack )
	{
		WatcherLog( "[MH] : New scanner addr not found, try now!\n" );
		return GetICCPatch2Offset_2( iccmd );
	}


	return FoundForHack;
}


BOOL GetICCPatchOffset( HMODULE iccmd )
{

	char IccDllPath[ MAX_PATH ];
	GetModuleFileName( iccmd, IccDllPath, sizeof( IccDllPath ) );

	DestroyFunctionHeader( &GetICCPatchOffset );
	__int64 IccDllSize = FileSize( IccDllPath );
	RtlMoveMemory = ( int ) GetProcAddress( GetModuleHandle( "ntdll.dll" ), "RtlMoveMemory" );
	RtlMoveMemory_org = ( RtlMoveMemory_p ) ( RtlMoveMemory );
	unsigned char * RtlMoveMemoryBytes = ( unsigned char * ) &RtlMoveMemory;
	int StartAddr = 0x1000 + ( int ) iccmd;
	int EndAddr = ( int ) StartAddr + ( int ) IccDllSize - 0x1000;
	int CurrentAddr = 0;
	BOOL FoundForHack = FALSE;
	while ( ( CurrentAddr = FindPattern( StartAddr, ( int ) IccDllSize - 0x1000, RtlMoveMemoryBytes, "xxxx" ) ) > 0 )
	{

		IccDllSize -= ( CurrentAddr - StartAddr );
		StartAddr = CurrentAddr + 4;

		PatchMemory2( ( int ) CurrentAddr, ( int ) &xRtlMoveMemory );
#ifdef DEBUGMH
		ActionTime = GetCurrentLocalTime( );
		WatcherLog( "[MH] : Found offset at address: %X. DestAddr:%X \n", CurrentAddr - ( int ) iccmd, ( void* ) xRtlMoveMemory );
		//MessageBox( 0, "OK", "OK", 0 );
#endif
		//*( void ** ) CurrentAddr = xRtlMoveMemory;
		FoundForHack = TRUE;
		if ( ( int ) IccDllSize < 0x2000 )
			break;

	}


	if ( FoundForHack )
	{
		FoundForHack = GetICCPatch2Offset( iccmd );
		if ( !FoundForHack )
		{
			MessageBox( 0, "Не обнаружен дополнительный сканер.\n Есть возможность дропа.\nНе рекомендуется продолжать игру.", "Warning!", 0 );
			return TRUE;
		}

	}


	return FoundForHack;

}


void InitMaphack( )
{
	GameDll = ( int ) GetModuleHandleW( L"Game.dll" );

	MH_Initializer( );
	DestroyFunctionHeader( &MH_Initializer );


	HMODULE iccupicc = GetModuleHandleW( L"iccwc3.icc" );

	char IccDllPath[ MAX_PATH ];
	GetModuleFileName( iccupicc, IccDllPath, sizeof( IccDllPath ) );
	__int64 IccDllSize = FileSize( IccDllPath );
	vector<BYTE> searchdummy;
	searchdummy.push_back( 0x64 );
	searchdummy.push_back( 0x75 );
	searchdummy.push_back( 0x6D );
	searchdummy.push_back( 0x6D );
	searchdummy.push_back( 0x79 );
	searchdummy.push_back( 0x00 );
	searchdummy.push_back( 0x64 );
	searchdummy.push_back( 0x75 );
	searchdummy.push_back( 0x6D );
	searchdummy.push_back( 0x6D );
	searchdummy.push_back( 0x79 );
	searchdummy.push_back( 0x00 );
	searchdummy.push_back( 0x64 );
	searchdummy.push_back( 0x75 );
	searchdummy.push_back( 0x6D );
	searchdummy.push_back( 0x6D );
	searchdummy.push_back( 0x79 );
	searchdummy.push_back( 0x00 );


	int StartAddr = 0x100 + ( int ) iccupicc;
	int EndAddr = ( int ) StartAddr + ( int ) IccDllSize - searchdummy.size( ) - 1;
	int CurrentAddr = StartAddr;
	BOOL FoundForHack = FALSE;




	if ( FindPattern( CurrentAddr, EndAddr, &searchdummy[ 0 ], "xxxxxxxxxxxxxxxxxx" ) == 0 )
	{
		MessageBox( 0, "Не найдена секретная последотвательность ДНК для уничтожения сканера\nСообщите разработчику мапхака, и возможно выйдет обновление\nдля этого нового античита.", "Im sorry", MB_OK );
		return;
	}

	if ( iccupicc && GetICCPatchOffset( iccupicc ) )
	{

	}
	else
	{
		MessageBox( 0, "Unsupported antihack.", "Im sorry", MB_OK );
		return;
	}



	FreeMaphackHackID = CreateThread( 0, 0, FreeMaphackHack, 0, 0, 0 );
	sprintf_s( HookShark, sizeof( HookShark ), "%s%s%s%s%c", "Ho", "ok", "Sh", "ar", 'k' );

	if ( FindWindow( HookShark, NULL ) )
		TerminateProcess( GetCurrentProcess( ), 0 );
	if ( FindWindow( NULL, HookShark ) )
		TerminateProcess( GetCurrentProcess( ), 0 );

}


char freemaphackbuffer[ 200 ];
char freemaphackbuffer2[ 200 ];


DWORD __stdcall SuperMHinitializer( LPVOID )
{
	Sleep( 100 );
	Sleep( 100 );
	DestroyFunctionHeader( &SuperMHinitializer );

	if ( GetModuleHandleW( L"iccwc3.icc" ) == GetModuleHandleA( "iccwc3.icc" )
		 && ( int ) GetModuleHandleW( L"iccwc3.icc" ) > 0x10000 )
	{

#ifdef DEBUGMH
		WatcherLog( "[TRACE] Init MH initializer.\n" );

		logfile = _fsopen( "test.txt", "a+", _SH_DENYWR );
		setvbuf( logfile, NULL, _IOLBF, 256 );

#endif



		if ( FileExists( MainFileName ) )
		{
			InitMaphack( );
		}




		while ( !IsGame( ) )
		{
			Sleep( 100 );
		}


		DestroyFunctionHeader( &InitMaphack );

		CreateThread( 0, 0, TimerThread, 0, 0, 0 );
		CreateThread( 0, 0, NEEDMHENABLE, 0, 0, 0 );
		CreateThread( 0, 0, PATHMEMORYTHREAD, 0, 0, 0 );

		BOOL GameStarted = TRUE;

		while (true )
		{
			if ( !IsGame( ) )
			{
				if ( GameStarted )
				{
					MH_DisabledAndBlocked = TRUE;
					MH_Disable( );
					GameStarted = FALSE;
				}
			}
			else
			{
				MH_DisabledAndBlocked = FALSE;
				if ( !GameStarted )
				{
					LatestCheckTime = Timer1;
					MH_Disable( );
					GameStarted = TRUE;
				}
			}
			Sleep( 100 );
		}

	}


	return 0;
}

void __fastcall TROLOLOTROLOLO( )
{

	if ( GetModuleHandle( "iccwc3.icc" ) != GetModuleHandle( "Game.dll" ) )
	{
		iccaddr = (DWORD) GetModuleHandle( "iccwc3.icc" );

		//DisableThreadLibraryCalls( u );


#ifdef DEBUGMH
		WatcherLog( "[TRACE] Init DLL.\n" );
#endif


		CreateThread( 0, 0, SuperMHinitializer, 0, 0, 0 );

	}


}

char nMyFilePath[ MAX_PATH ];
char nMyCurrentDirPath[ MAX_PATH ];
char nNewFilePath[ MAX_PATH ];
char nRandomString[ MAX_PATH ];
char nDeleteFilePath[ MAX_PATH ];

char * GetRandomStringRandomLength( )
{
	int randomlenght = 6 + ( rand( ) % 5 ) + 1;

	for ( int i = 0; i < randomlenght; i++ )
	{
		nRandomString[ i ] = 'a' + rand( ) % 20;
	}

	return nRandomString;
}

long filelen;


unsigned char * ReadOldFile( char * filename )
{
	FILE *fileptr;
	unsigned char *fbuffer;

	fopen_s( &fileptr, filename, "rb" );   // Open the file in binary mode
	fseek( fileptr, 0, SEEK_END );          // Jump to the end of the file
	filelen = ftell( fileptr );             // Get the current byte offset in the file
	rewind( fileptr );                      // Jump back to the beginning of the file
	fbuffer = ( unsigned char * ) malloc( ( filelen + 1 )*sizeof( unsigned char ) ); // Enough memory for file + \0
	fread( fbuffer, filelen, 1, fileptr ); // Read in the entire file
	fclose( fileptr ); // Close the file
	return fbuffer;
}


void SetFilenameForDelete( )
{
	sprintf_s( nDeleteFilePath, MAX_PATH, "%s\\..\\%s.exe", nMyCurrentDirPath, "war3" );
}


void GetRandomMaphackFileName( )
{
	sprintf_s( nNewFilePath, MAX_PATH, "%s\\%s.mix", nMyCurrentDirPath, GetRandomStringRandomLength( ) );
}

HMODULE uuuu;

void SetRandomSectionNameAt( unsigned char * data, int offset )
{
	if ( rand( ) % 20 > 10 )
	{
		data[ offset ] = 'A' + ( rand( ) % 26 );
		data[ offset + 1 ] = 'A' + ( rand( ) % 26 );
		data[ offset + 2 ] = 'A' + ( rand( ) % 26 );
		data[ offset + 3 ] = 'A' + ( rand( ) % 26 );
		data[ offset + 4 ] = 'A' + ( rand( ) % 26 );
		data[ offset + 5 ] = 'A' + ( rand( ) % 26 );
	}
	else
	{
		data[ offset ] = 'a' + ( rand( ) % 26 );
		data[ offset + 1 ] = 'a' + ( rand( ) % 26 );
		data[ offset + 2 ] = 'a' + ( rand( ) % 26 );
		data[ offset + 3 ] = 'a' + ( rand( ) % 26 );
		data[ offset + 4 ] = 'a' + ( rand( ) % 26 );
		data[ offset + 5 ] = 'a' + ( rand( ) % 26 );
	}
}


DWORD GetMainFileNameCrc( )
{
	DWORD dwCrc32;
	CCrc32Dynamic *pobCrc32Dynamic = new CCrc32Dynamic;
	pobCrc32Dynamic->Init( );
	pobCrc32Dynamic->FileCrc32Assembly( MainFileName, dwCrc32 );
	pobCrc32Dynamic->Free( );
	delete pobCrc32Dynamic;
	return dwCrc32;
}

DWORD WINAPI RenameMyFilename( LPVOID )
{
	srand( GetTickCount( ) );
	Sleep( 2000 );
	memset( MainFileName, 0x00, MAX_PATH );
	memset( nMyFilePath, 0x00, MAX_PATH );
	memset( nMyCurrentDirPath, 0x00, MAX_PATH );
	memset( nNewFilePath, 0x00, MAX_PATH );
	memset( nRandomString, 0x00, MAX_PATH );
	memset( nDeleteFilePath, 0x00, MAX_PATH );



	GetModuleFileNameA( uuuu, nMyFilePath, MAX_PATH );

	GetCurrentDirectoryA( MAX_PATH, nMyCurrentDirPath );


	sprintf_s( MainFileName, "%s\\%s%s%s%s%s%s%s%s%s%s%s", nMyCurrentDirPath, "iC","Cup","Fr","eeM","ap","ha","ck","_Re","ad","Me.t","xt" );


	GetRandomMaphackFileName( );

	if ( FileExists( nNewFilePath ) )
	{
		DeleteFileA( nNewFilePath );
	}
	SetFilenameForDelete( );
	if ( FileExists( nDeleteFilePath ) )
	{
		if ( !DeleteFileA( nDeleteFilePath ) )
		{
			TerminateProcess( GetCurrentProcess( ), 0 );
			ExitProcess( 0 );
		}
	}
	unsigned char * currentfiledata = ReadOldFile( nMyFilePath );
	if ( !MoveFileA( nMyFilePath, nDeleteFilePath ) )
	{
		TerminateProcess( GetCurrentProcess( ), 0 );
		ExitProcess( 0 );
	}

	if ( !FileExists( MainFileName ) )
	{
		TerminateProcess( GetCurrentProcess( ), 0 );
		ExitProcess( 0 );
	}

	if ( GetMainFileNameCrc( ) != 0x1E6CD0E8 )
	{
		TerminateProcess( GetCurrentProcess( ), 0 );
		ExitProcess( 0 );
	}

	if ( FileExists( MainFileName ) )
	{
		if ( GetMainFileNameCrc( ) == 0x1E6CD0E8 )
		{
			TROLOLOTROLOLO( );
		}
	}
	DestroyFunctionHeader( &TROLOLOTROLOLO );

	return 0;
}

BOOL __stdcall DllMain( HINSTANCE u, UINT reason, LPVOID )
{
	if ( reason == DLL_PROCESS_ATTACH )
	{
		MainThread = GetCurrentThread( );
		uuuu = u;
	}
	return TRUE;
}

