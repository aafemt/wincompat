/*********** Implementation of WideCharToMultiByte function ************/

#include <map>
#include <iconv.h>

#include "stringapiset.h"
#include "winerror.h"
#include "winnls.h"

namespace
{

	int WideCharToUtf8(DWORD dwFlags, const wchar_t* lpWideCharStr, int cchWideChar, char* lpMultiByteStr, int cbMultiByte, BOOL* lpUsedDefaultChar)
	{
		int result = 0;
		bool invalidCharacters = false;
		do
		{
			wchar_t c = *lpWideCharStr++;
			int len;
			if (c <= 0x7f)
			{
				len = 1;
			}
			else if (c <= 0x7ff)
			{
				len = 2;
			}
			else if ((c >= 0xd800 && c <= 0xdfff) || c > 0x10ffff)
			{
				// Do it immediatelly to make sure that it is set even if error ERROR_INSUFFICIENT_BUFFER is returned
				if (lpUsedDefaultChar != nullptr)
				{
					*lpUsedDefaultChar = TRUE;
				}
				if (dwFlags & WC_ERR_INVALID_CHARS)
				{
					invalidCharacters = true;
					if (cbMultiByte == 0)
					{
						// No point to count further: no resul is returned
						goto exit;
					}
					// But if the real conversion requested - it still must be completed
				}
				len = 3;
				c = 0xFFFD;
			}
			else if (c <= 0xffff)
			{
				len = 3;
			}
			else
			{
				len = 4;
			}

			result += len;
			if (cbMultiByte == 0)
			{
				if (cchWideChar == -1 && c == 0)
				{
					goto exit;
				}
			}
			else
			{
				if (len > cbMultiByte)
				{
					errno = ERROR_INSUFFICIENT_BUFFER;
					return 0;
				}

				switch (len)
				{
				case 1:
					{
						*lpMultiByteStr++ = c;
						if (cchWideChar == -1 && c == 0)
						{
							goto exit;
						}
						break;
					}
				case 2:
					{
						*lpMultiByteStr++ = 0xc0 | (c >> 6);
						*lpMultiByteStr++ = 0x80 | (c & 0x3f);
						break;
					}
				case 3:
					{
						*lpMultiByteStr++ = 0xe0 | (c >> 12);
						*lpMultiByteStr++ = 0x80 | ((c >> 6) & 0x3f);
						*lpMultiByteStr++ = 0x80 | (c & 0x3f);
						break;
					}
				case 4:
					{
						*lpMultiByteStr++ = 0xf0 | (c >> 18);
						*lpMultiByteStr++ = 0x80 | ((c >> 12) & 0x3f);
						*lpMultiByteStr++ = 0x80 | ((c >> 6) & 0x3f);
						*lpMultiByteStr++ = 0x80 | (c & 0x3f);
					}
				}
				cbMultiByte -= len;
			}
		}
		while (cchWideChar == -1 || --cchWideChar > 0);
	exit:
		if (invalidCharacters)
		{
			errno = ERROR_NO_UNICODE_TRANSLATION;
			return 0;
		}
		return result;
	}

//	int WideCharToSystem(DWORD dwFlags, const wchar_t* lpWideCharStr, int cchWideChar, char* lpMultiByteStr, int cbMultiByte);
	int WideCharToAny(unsigned CodePage, DWORD dwFlags,
						const wchar_t* lpWideCharStr, int cchWideChar,
						char* lpMultiByteStr, int cbMultiByte,
						const char* lpDefaultChar, BOOL* lpUsedDefaultChar)
	{
		static const std::map<unsigned, const char*> codePages
		{
			{ 0,	"" }, // ANSI is a defaul locale, right?
			{ 37,	"IBM037" },
			{ 437,	"IBM437" },
			{ 500,	"IBM500" },
			{ 708,	"ASMO-708" },
			{ 709,	"ASMO_449" },
			{ 710,	"ARABIC" }, // ??
			{ 720,	"ARABIC" }, // ??
			{ 737,	"CP737" },
			{ 775,	"CP775" },
			{ 850,	"CP850" },
			{ 852,	"CP852" },
			{ 855,	"CP855" },
			{ 857,	"CP857" },
			{ 858,	"CP858" },
			{ 860,	"CP860" },
			{ 861,	"CP861" },
			{ 862,	"CP862" },
			{ 863,	"CP863" },
			{ 864,	"CP864" },
			{ 865,	"CP865" },
			{ 866,	"CP866" },
			{ 869,	"CP869" },
			{ 870,	"CP870"	},
			{ 874,	"CP874" },
			{ 875,	"CP875" },
			{ 932,	"CP932" },
			{ 936,	"CP936" },
			{ 949,	"CP949" },
			{ 950,	"CP950" },
			{ 1026,	"CP1026" },
			{ 1047, "CP1047" },
			{ 1140,	"CP1140" },
			{ 1141,	"CP1141" },
			{ 1145, "CP1145" },
			{ 1146,	"CP1146" },
			{ 1147,	"CP1147" },
			{ 1148,	"CP1148" },
			{ 1149,	"CP1149" },
			{ 1200,	"UTF16LE" },
			{ 1201,	"UTF16BE" },
			{ 1250,	"CP1250" },
			{ 1251, "CP1251" },
			{ 1252, "CP1252" },
			{ 1253,	"CP1253" },
			{ 1254,	"CP1254" },
			{ 1255,	"CP1255" },
			{ 1256,	"CP1256" },
			{ 1257,	"CP1257" },
			{ 1258,	"CP1258" },
			{ 1361,	"CP1361" },
			{ 10000,	"MACINTOSH" },
			{ 10001,	"MACJAPANESE" }, // ??
			{ 10007,	"MACCYRILLIC" },
			{ 10017,	"MACUKRAINIAN" },
			{ 10029,	"MAC-CENTRALEUROPE" },
			{ 12000,	"UTF32LE" },
			{ 12001,	"UTF32BE" },
			{ 20172,	"US-ASCII" },
			{ 20261,	"T.61" },
			{ 20269,	"ISO6937" },
			{ 20273,	"IBM273" },
			{ 20277,	"IBM277" },
			{ 20278,	"IBM278" },
			{ 20280,	"IBM280" },
			{ 20284,	"IBM284" },
			{ 20285,	"IBM285" },
			{ 20290,	"IBM290" },
			{ 20297,	"IBM297" },
			{ 20420,	"IBM420" },
			{ 20423,	"IBM423" },
			{ 20866,	"KOI8-R" },
			{ 20871,	"IBM871" },
			{ 20880,	"IBM880" },
			{ 20905,	"IBM905" },
			{ 20932,	"EUCJP" },
			{ 20936,	"GB2312" },
			{ 21025,	"IBM1025" },
			{ 21866,	"KOI8-U" },
			{ 28591,	"ISO88591" },
			{ 28592,	"ISO88592" },
			{ 28593,	"ISO88593" },
			{ 28594,	"ISO88594" },
			{ 28595,	"ISO88595" },
			{ 28596,	"ISO88596" },
			{ 28597,	"ISO88597" },
			{ 28598,	"ISO88598" },
			{ 28599,	"ISO88599" },
			{ 28603,	"ISO885913" },
			{ 28605,	"ISO885915" },
			{ 38598,	"ISO88598" },
			{ 50220,	"ISO2022JP" },
			{ 50221,	"CSISO2022JP2" },
			{ 50225,	"ISO2022KR" },
			{ 50229,	"ISO2022CN" },
			{ 50930,	"IBM930" },
			{ 50933,	"IBM933" },
			{ 50935,	"IBM935" },
			{ 50936,	"CP936" },
			{ 50937,	"IBM937" },
			{ 50939,	"IBM939" },
			{ 51932,	"EUCJP" },
			{ 51936,	"EUCCN" },
			{ 61549,	"EUCKR" },
			{ 54936,	"GB18030" },
			{ 65000,	"UTF7" },
			{ 65001,	"UTF8" }
		};

		auto itr = codePages.find(CodePage);
		if (itr == codePages.end())
		{
			errno = ERROR_INVALID_PARAMETER;
			return 0;
		}

		if (cchWideChar == -1)
		{
			cchWideChar = wcslen(lpWideCharStr) + 1; // Include zero terminator as well
		}

		int result = 0;
		bool invalidCharacters = false;
		iconv_t conv = iconv_open(itr->second, "WCHAR_T");
		if (conv == (iconv_t)(-1))
		{
			return 0;
		}

		char* inbuf = const_cast<char*>(reinterpret_cast<const char*>(lpWideCharStr));
		size_t inbytesleft = cchWideChar * sizeof(wchar_t);

		if (lpMultiByteStr == nullptr)
		{
			// Just calculate number of needed bytes.
			// It is pity that iconv cannot operate without output buffer.
			char buf[1024];

			while (inbytesleft > 0)
			{
				char* p = buf;
				size_t l = sizeof(buf);
				size_t res = iconv(conv, &inbuf, &inbytesleft, &p, &l);
				if (res == (size_t)-1)
				{
					if (errno == EILSEQ)
					{
						// Count phantom default character
						invalidCharacters = true;
						p = buf + 1;
						inbuf += sizeof(wchar_t);
						inbytesleft -= sizeof(wchar_t);
					}
					else if (errno != E2BIG)
					{
						// Reset conversion state to prevent dirty output on the next call
						result = 0;
						break;
					}
				}
				result += p - buf;
			}
		}
		else
		{
			char* outbuf = lpMultiByteStr;
			size_t outbytesleft = cbMultiByte;

			while (inbytesleft > 0)
			{
				size_t res = iconv(conv, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
				if (res == (size_t)-1)
				{
					if (errno == EILSEQ)
					{
						// Special handling for case when invalid character would overflow output buffer
						// It seems that iconv has priority of EILSEQ higher than E2BIG
//						if (outbytesleft == 0)
//						{
//							result = 0;
//							errno = ERROR_INSUFFICIENT_BUFFER;
//							break;
//						}
						if (lpUsedDefaultChar != nullptr)
						{
							*lpUsedDefaultChar = TRUE;
						}
						if (lpDefaultChar != nullptr)
						{
							*outbuf++ = *lpDefaultChar;
						}
						else
						{
							// Perhaps here must be conversion of U+FFFD instead...
							*outbuf++ = '?';
						}
						--outbytesleft;
						inbuf += sizeof(wchar_t);
						inbytesleft -= sizeof(wchar_t);
					}
					else // even if (errno == E2BIG)
					{
						result = 0;
						break;
					}
				}
				result = outbuf - lpMultiByteStr;
			}
		}

		iconv_close(conv);

		if (invalidCharacters)
		{
			if (lpUsedDefaultChar != nullptr)
			{
				*lpUsedDefaultChar = TRUE;
			}
			if (dwFlags & WC_ERR_INVALID_CHARS)
			{
				result = 0;
			}
		}

		return result;
	}
}


int WideCharToMultiByte(unsigned CodePage, DWORD dwFlags,
						const wchar_t* lpWideCharStr, int cchWideChar,
						char* lpMultiByteStr, int cbMultiByte,
						const char* lpDefaultChar, BOOL* lpUsedDefaultChar)
{
	// Sanity checks first
	if (cchWideChar == 0 || lpWideCharStr == nullptr
		|| (lpMultiByteStr == nullptr && cbMultiByte != 0)
		|| (void*)lpWideCharStr == (void*)lpMultiByteStr)
	{
		errno = ERROR_INVALID_PARAMETER;
		return 0;
	}

	if (lpUsedDefaultChar != nullptr)
	{
		*lpUsedDefaultChar = FALSE;
	}

	// Special handling for CP_UTF8 and CP_ACP
	switch (CodePage)
	{
	case CP_UTF8:
		{
			return WideCharToUtf8(dwFlags, lpWideCharStr, cchWideChar, lpMultiByteStr, cbMultiByte, lpUsedDefaultChar);
		}
/*
	case CP_ACP:
		{
			return WideCharToSystem(dwFlags, lpWideCharStr, cchWideChar, lpMultiByteStr, cbMultiByte);
		}
*/
	default:
		{
			return WideCharToAny(CodePage, dwFlags, lpWideCharStr, cchWideChar, lpMultiByteStr, cbMultiByte, lpDefaultChar, lpUsedDefaultChar);
		}
	}
}
