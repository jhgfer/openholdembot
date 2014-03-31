//***************************************************************************** 
//
// This file is part of the OpenHoldem project
//   Download page:         http://code.google.com/p/openholdembot/
//   Forums:                http://www.maxinmontreal.com/forums/index.php
//   Licensed under GPL v3: http://www.gnu.org/licenses/gpl.html
//
//***************************************************************************** 
//
// Purpose:
//
//***************************************************************************** 

#include "stdafx.h"
#include "debug.h"

#include "..\CTablemap\CTablemap.h"
#include "..\..\dbghelp\dbghelp.h"
#include "CAutoplayerFunctions.h"
#include "CBetroundCalculator.h"
#include "CEngineContainer.h"
#include "CFilenames.h"
#include "CIteratorThread.h"
#include "CIteratorVars.h"
#include "CPreferences.h"
#include "CScraper.h"
#include "CSymbolEngineAutoplayer.h"
#include "CSymbolEngineChipAmounts.h"
#include "CSymbolEngineHandrank.h"
#include "CSymbolEnginePokerval.h"
#include "CSymbolEnginePrwin.h"
#include "CSymbolEngineUserchair.h"
#include "inlines/eval.h"
#include "OH_MessageBox.h"
#include "OpenHoldem.h"
#include <sys/stat.h>


//#include <vld.h>			// visual leak detector

FILE *log_fp = NULL;
CCritSec log_critsec;  // Used to ensure only one thread at a time writes to log file

char * get_time(char * timebuf) 
{
    // returns current system time in WH format
    time_t	ltime;
    char tmptime[30];

    time( &ltime );
    ctime_s(tmptime, 26, &ltime);
    tmptime[24]='\0';

    memcpy(timebuf, tmptime+20, 4); //yyyy
    *(timebuf+4) = '-';

    // mm
    if (memcmp(tmptime+4, "Jan", 3)==0)  {
        *(timebuf+5) = '0';
        *(timebuf+6) = '1';
    }
    else if (memcmp(tmptime+4, "Feb", 3)==0)  {
        *(timebuf+5) = '0';
        *(timebuf+6) = '2';
    }
    else if (memcmp(tmptime+4, "Mar", 3)==0)  {
        *(timebuf+5) = '0';
        *(timebuf+6) = '3';
    }
    else if (memcmp(tmptime+4, "Apr", 3)==0)  {
        *(timebuf+5) = '0';
        *(timebuf+6) = '4';
    }
    else if (memcmp(tmptime+4, "May", 3)==0)  {
        *(timebuf+5) = '0';
        *(timebuf+6) = '5';
    }
    else if (memcmp(tmptime+4, "Jun", 3)==0)  {
        *(timebuf+5) = '0';
        *(timebuf+6) = '6';
    }
    else if (memcmp(tmptime+4, "Jul", 3)==0)  {
        *(timebuf+5) = '0';
        *(timebuf+6) = '7';
    }
    else if (memcmp(tmptime+4, "Aug", 3)==0)  {
        *(timebuf+5) = '0';
        *(timebuf+6) = '8';
    }
    else if (memcmp(tmptime+4, "Sep", 3)==0)  {
        *(timebuf+5) = '0';
        *(timebuf+6) = '9';
    }
    else if (memcmp(tmptime+4, "Oct", 3)==0)  {
        *(timebuf+5) = '1';
        *(timebuf+6) = '0';
    }
    else if (memcmp(tmptime+4, "Nov", 3)==0)  {
        *(timebuf+5) = '1';
        *(timebuf+6) = '1';
    }
    else if (memcmp(tmptime+4, "Dec", 3)==0)  {
        *(timebuf+5) = '1';
        *(timebuf+6) = '2';
    }

    *(timebuf+7) = '-';
    memcpy(timebuf+8, tmptime+8, 2); //dd
    *(timebuf+10) = ' ';
    memcpy(timebuf+11, tmptime+11, 8); //HH:mm:ss
    *(timebuf+19) = '\0';

    return timebuf;
}

char * get_now_time(char * timebuf) 
{
    // returns current system time as a UNIX style string
    time_t	ltime;

    time( &ltime );
    ctime_s(timebuf, 26, &ltime);
    timebuf[24]='\0';

    return timebuf;
}

LONG WINAPI MyUnHandledExceptionFilter(EXCEPTION_POINTERS *pExceptionPointers) 
{
	// Create a minidump
	GenerateDump(pExceptionPointers);

	OH_MessageBox_Error_Warning(
		"OpenHoldem is about to crash.\n"
		"A minidump has been created in your\n"
		"OpenHoldem startup directory.\n"
		"\n"
		"OpenHoldem will shut down when you click OK.",
		"FATAL ERROR");
    return EXCEPTION_EXECUTE_HANDLER;
}

BOOL CreateBMPFile(const char *szFile, HBITMAP hBMP) 
{
    // Saves the hBitmap as a bitmap.
    HDC					hdcScreen = CreateDC("DISPLAY", NULL, NULL, NULL);
    HDC					hdcCompatible = CreateCompatibleDC(hdcScreen);
    PBITMAPINFO			pbmi=NULL;
    BOOL				bret=FALSE;
    HANDLE				hf=NULL; // file handle
    BITMAPFILEHEADER	hdr; // bitmap file-header
    PBITMAPINFOHEADER	pbih=NULL; // bitmap info-header
    LPBYTE				lpBits=NULL; // memory pointer
    DWORD				dwTotal=0; // total count of bytes
    DWORD				cb=0; // incremental count of bytes
    BYTE				*hp=NULL; // byte pointer
    DWORD				dwTmp=0;
    BITMAP				bmp;

    memset(&bmp,0,sizeof(BITMAP));
    GetObject(hBMP,sizeof(BITMAP),&bmp);
    memset(&hdr,0,sizeof(hdr));
    {
        int cClrBits = (WORD)(bmp.bmPlanes * bmp.bmBitsPixel);
        if (cClrBits>8) {
            // No Palette (normally)
            pbmi = (PBITMAPINFO) calloc(1, sizeof(BITMAPINFOHEADER));
        }
        else {
            pbmi = (PBITMAPINFO) calloc(1, sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * (1<<(min(8,cClrBits))));
            pbmi->bmiHeader.biClrUsed = (1<<cClrBits);
        }

        // Initialize the fields in the BITMAPINFO structure.
        pbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

        // Retrieve the color table (RGBQUAD array) and the bits
        // (array of palette indices) from the DIB.
        if (!GetDIBits(hdcCompatible, hBMP, 0, bmp.bmHeight, NULL, pbmi, DIB_RGB_COLORS)) {
            goto to_return;
        }
    }
    pbih = &(pbmi->bmiHeader);
    pbmi->bmiHeader.biCompression=BI_RGB;
    lpBits = (LPBYTE) calloc(1, pbih->biSizeImage);

    if (!lpBits) {
        goto to_return;
    }

    if (!GetDIBits(hdcCompatible, hBMP, 0, (WORD) pbih->biHeight, lpBits, pbmi, DIB_RGB_COLORS)) {
        goto to_return;
    }

    // Create the .BMP file.
    hf = CreateFile(szFile, GENERIC_READ | GENERIC_WRITE, (DWORD) 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, (HANDLE) NULL);
    if (hf == INVALID_HANDLE_VALUE) {
        goto to_return;
    }
    hdr.bfType = 0x4d42; // 0x42 = "B" 0x4d = "M"
    // Compute the size of the entire file.
    hdr.bfSize = (DWORD) (sizeof(BITMAPFILEHEADER) + pbih->biSize + pbih->biClrUsed*sizeof(RGBQUAD) + pbih->biSizeImage);
    hdr.bfReserved1 = 0;
    hdr.bfReserved2 = 0;

    // Compute the offset to the array of color indices.
    hdr.bfOffBits = (DWORD) sizeof(BITMAPFILEHEADER) + pbih->biSize + pbih->biClrUsed*sizeof (RGBQUAD);

    // Copy the BITMAPFILEHEADER into the .BMP file.
    if (!WriteFile(hf, (LPVOID) &hdr, sizeof(BITMAPFILEHEADER),	(LPDWORD) &dwTmp, NULL)) {
        goto to_return;
    }

    // Copy the BITMAPINFOHEADER and RGBQUAD array into the file.
    if (!WriteFile(hf, (LPVOID) pbih, sizeof(BITMAPINFOHEADER) + pbih->biClrUsed * sizeof (RGBQUAD), (LPDWORD) &dwTmp, ( NULL))) {
        goto to_return;
    }

    // Copy the array of color indices into the .BMP file.
    dwTotal = cb = pbih->biSizeImage;
    hp = lpBits;
    if (!WriteFile(hf, (LPSTR) hp, (int) cb, (LPDWORD) &dwTmp,NULL)) {
        goto to_return;
    }

    // Close the .BMP file.
    if (!CloseHandle(hf)) {
        goto to_return;
    }
    bret=TRUE;


to_return:
    ;
    // Free memory.
    if (pbmi)free(pbmi);
    if (lpBits)free(lpBits);
    DeleteDC(hdcCompatible);
    DeleteDC(hdcScreen);

    return bret;
}

void start_log(void) 
{
	if (log_fp!=NULL)
		return;

	CSLock lock(log_critsec);

	CString fn = p_filenames->LogFilename();
	// Check, if file exists and size is too large
	struct stat file_stats = { 0 };
	if (stat(fn.GetString(), &file_stats) == 0)
	{
		unsigned long int max_file_size = 1E06 * preferences.log_max_logsize();
		size_t file_size = file_stats.st_size;
		if (file_size > max_file_size)
		{
			remove(fn.GetString());
		}
	}

	// Append (or create) log
	if ((log_fp = _fsopen(fn.GetString(), "a", _SH_DENYWR)) != 0)
	{
		write_log(k_always_log_basic_information, "\n%s%s%s%s%s%s",
			"*************************************************************\n",
			"OpenHoldem ", VERSION_TEXT, "\n",
			"LOG FILE OPEN\n",
			"*************************************************************\n");
		fflush(log_fp);
	}

}

void write_log_vl(bool debug_settings_for_this_message, char* fmt, va_list vl) 
{
    char		buff[10000] ;
    char		nowtime[26];

	if (debug_settings_for_this_message == false)
		return;

    if (log_fp != NULL) 
	{
		CSLock lock(log_critsec);

        vsprintf_s(buff, 10000, fmt, vl);
		get_time(nowtime);
        fprintf(log_fp, "%s > %s", nowtime, buff);

        fflush(log_fp);
    }
}

void write_log(bool debug_settings_for_this_message, char* fmt, ...) 
{
    char		buff[10000] ;
    va_list		ap;
    char		nowtime[26];

	if (debug_settings_for_this_message == false)
		return;

    if (log_fp != NULL) 
	{
		CSLock lock(log_critsec);

        va_start(ap, fmt);
        vsprintf_s(buff, 10000, fmt, ap);
		get_time(nowtime);
        fprintf(log_fp, "%s - %s", nowtime, buff);

        va_end(ap);

        fflush(log_fp);
    }
}

void write_log_nostamp(bool debug_settings_for_this_message, char* fmt, ...) 
{
	char		buff[10000] ;
    va_list		ap;

	if (debug_settings_for_this_message == false)
		return;

    if (log_fp != NULL) 
	{
		CSLock lock(log_critsec);

        va_start(ap, fmt);
        vsprintf_s(buff, 10000, fmt, ap);
        fprintf(log_fp, "%s", buff);

        va_end(ap);

        fflush(log_fp);
    }
}

void write_logautoplay(const char * action) 
{
    char		nowtime[26];
    CString		pcards, comcards, temp, rank, pokerhand, bestaction;
    char		*card;
    CardMask	Cards;
    int			nCards;
    CString		fcra_formula_status;

	int			userchair = p_symbol_engine_userchair->userchair();
	int			betround  = p_betround_calculator->betround();

	if (!preferences.trace_enabled())
		return;

	if (log_fp != NULL) 
	{
		CSLock lock(log_critsec);

		// log$ writing
		if (preferences.log_symbol_enabled())
		{
			int max_log = p_engine_container->logsymbols_collection()->GetCount();

			if (max_log > 0)
			{
				if (max_log > preferences.log_symbol_max_log())
				{
					max_log = preferences.log_symbol_max_log();
				}

				write_log(k_always_log_basic_information, "*** log$ (Total: %d | Showing: %d)\n", p_engine_container->logsymbols_collection()->GetCount(), max_log);

				for (int i=0; i<max_log; i++)
				{
					write_log(k_always_log_basic_information, "***     %s\n", p_engine_container->logsymbols_collection()->GetAt(i));
				}
			}
		}
		
		CardMask_RESET(Cards);
		nCards=0;

		// player cards
		if (p_symbol_engine_userchair->userchair_confirmed()) 
		{
			for (int i=0; i<=1; i++) 
			{
				card = StdDeck_cardString(p_scraper->card_player(userchair, i));
				temp.Format("%s", card);
				pcards.Append(temp);
				CardMask_SET(Cards, p_scraper->card_player(userchair, i));
				nCards++;
			}
		}
		else 
		{
			pcards = "....";
		}

		// common cards
		comcards = "";
		if (betround >= k_betround_flop) 
		{
			for (int i=0; i<=2; i++) 
			{
				if (p_scraper->card_common(i) != CARD_BACK && 
					p_scraper->card_common(i) != CARD_NOCARD) 
				{
					card = StdDeck_cardString(p_scraper->card_common(i));
					temp.Format("%s", card);
					comcards.Append(temp);
					CardMask_SET(Cards, p_scraper->card_common(i));
					nCards++;
				}
			}
		}

		if (betround >= k_betround_turn) 
		{
			card = StdDeck_cardString(p_scraper->card_common(3));
			temp.Format("%s", card);
			comcards.Append(temp);
			CardMask_SET(Cards, p_scraper->card_common(3));
			nCards++;
		}

		if (betround >= k_betround_river) 
		{
			card = StdDeck_cardString(p_scraper->card_common(4));
			temp.Format("%s", card);
			comcards.Append(temp);
			CardMask_SET(Cards, p_scraper->card_common(4));
			nCards++;
		}

        comcards.Append("..........");
        comcards = comcards.Left(10);

        // Always use handrank169 here
		rank.Format("%.0f", p_symbol_engine_handrank->handrank169());

        // poker hand
		pokerhand = p_symbol_engine_pokerval->HandType();

        // best action
		// !!! needs to be extended for betpot, etc.
        if (p_autoplayer_functions->f$alli())
            bestaction = "Allin";
        else if ((strcmp(action, "SWAG")==0) 
			|| (p_autoplayer_functions->f$betsize() > 0)) 
				bestaction.Format("Raise to $%.2f", p_autoplayer_functions->f$betsize());				
        else if (p_autoplayer_functions->f$rais())
            bestaction = "Bet/Raise";
        else if (p_autoplayer_functions->f$call())
            bestaction = "Call/Check";
        else if (p_autoplayer_functions->f$prefold())
            bestaction = "Prefold";
        else
            bestaction = "Check/Fold";

        // fcra_seen
		CString fcra_seen = p_symbol_engine_autoplayer->GetFCKRAString();
        // fcra formula status
		fcra_formula_status.Format("%s%s%s%s%s",
			p_autoplayer_functions->f$fold() ? "F" : ".",
			p_autoplayer_functions->f$call() ? "C" : ".",
			p_autoplayer_functions->f$call() ? "K" : ".",
			p_autoplayer_functions->f$rais() ? "R" : ".",
			p_autoplayer_functions->f$alli() ? "A" : ".");
		
		// More verbose summary in the log
		// The old WinHoldem format was a complete mess
		fprintf(log_fp, get_time(nowtime));
		fprintf(log_fp, "**** Basic Info *********************************************\n");
		fprintf(log_fp, "  Version:       %s\n",    VERSION_TEXT); 
		fprintf(log_fp, "  Chairs:		  %6d\n",   p_tablemap->nchairs());
		fprintf(log_fp, "  Userchair:     %6d\n",   userchair);
		fprintf(log_fp, "  Holecards:     %s\n",    pcards.GetString());
		fprintf(log_fp, "  Community:     %s\n",    comcards.GetString());
		fprintf(log_fp, "  Handrank:      %s\n",    rank.GetString());
		fprintf(log_fp, "  Hand:          %s\n",    pokerhand.GetString());
		//fprintf(log_fp, "  PrWin:         %6d\n",   (iter_vars.prwin() * 1000));
		//fprintf(log_fp, "  PrLos:         %6d\n",   (iter_vars.prlos() * 1000));
		//fprintf(log_fp, "  PrTie:         %6d\n",   (iter_vars.prtie() * 1000));
		//fprintf(log_fp, "  NOpponents:    %6d\n",   p_symbol_engine_prwin->nopponents_for_prwin());
		//fprintf(log_fp, "  Iterations:    %6d\n",   iter_vars.nit());
		fprintf(log_fp, "  My balance:    %9.2f\n", p_symbol_engine_chip_amounts->balance(userchair));
		fprintf(log_fp, "  My currentbet: %9.2f\n", p_symbol_engine_chip_amounts->currentbet(userchair)); 
		fprintf(log_fp, "  To call:       %9.2f\n", p_symbol_engine_chip_amounts->call());
		fprintf(log_fp, "  Pot:           %9.2f\n", p_symbol_engine_chip_amounts->pot());
		fprintf(log_fp, "  Big blind:     %9.2f\n", p_symbol_engine_tablelimits->bblind());
		fprintf(log_fp, "  Big bet (FL):  %9.2f\n", p_symbol_engine_tablelimits->bigbet());
		fprintf(log_fp, "  f$betsize:     %9.2f\n", p_autoplayer_functions->f$betsize());
		fprintf(log_fp, "  Formulas:      %s\n",    fcra_formula_status.GetString());
		fprintf(log_fp, "  Buttons:       %s\n",    fcra_seen.GetString());
		fprintf(log_fp, "  Best action:   %s\n",    bestaction.GetString());
		fprintf(log_fp, "  Action taken:  %s\n",    action);

		if (preferences.trace_enabled() && p_engine_container->symboltrace_collection()->GetSize() > 0)
		{
			write_log_nostamp(1, "***** Autoplayer Trace **************************************\n");
			for (int i=0; i<p_engine_container->symboltrace_collection()->GetSize(); i++)
			{
				write_log_nostamp(1, "%s\n", p_engine_container->symboltrace_collection()->GetAt(i));
			}
			write_log_nostamp(1, "***** History (might be not accurate) ***********************\n");
		}

		fflush(log_fp);
    }
}

void stop_log(void) 
{
    if (log_fp != NULL) 
	{
        write_log(k_always_log_basic_information, "\n%s%s%s%s%s%s",
			"*************************************************************\n",
			"OpenHoldem ", VERSION_TEXT, "\n",
			"LOG FILE CLOSED\n",
			"*************************************************************\n");
        fclose(log_fp);
        log_fp = NULL;
    }
}

int GenerateDump(EXCEPTION_POINTERS *pExceptionPointers)
{
    bool		bMiniDumpSuccessful;
    DWORD		dwBufferSize = MAX_PATH;
    HANDLE		hDumpFile;
    
    MINIDUMP_EXCEPTION_INFORMATION	ExpParam;

	hDumpFile = CreateFile(p_filenames->MiniDumpFilename(), 
		GENERIC_READ|GENERIC_WRITE, FILE_SHARE_WRITE|FILE_SHARE_READ, 
		0, CREATE_ALWAYS, 0, 0);
   
    ExpParam.ThreadId = GetCurrentThreadId();
    ExpParam.ExceptionPointers = pExceptionPointers;
    ExpParam.ClientPointers = TRUE;

	MINIDUMP_TYPE	mdt = (MINIDUMP_TYPE) (MiniDumpWithPrivateReadWriteMemory | 
										   MiniDumpWithDataSegs | 
										   MiniDumpWithHandleData |
										   MiniDumpWithFullMemoryInfo | 
										   MiniDumpWithThreadInfo | 
										   MiniDumpWithUnloadedModules);
   
    bMiniDumpSuccessful = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hDumpFile,
											mdt, &ExpParam, NULL, NULL);

    return EXCEPTION_EXECUTE_HANDLER;
}
		