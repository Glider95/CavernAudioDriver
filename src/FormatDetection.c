/***************************************************************************
 * FormatDetection.c
 * 
 * Dolby Atmos format detection implementation
 ***************************************************************************/

#include <ntddk.h>
#include "..\include\FormatDetection.h"

/**************************************************************************
 * CavernDetectFormat
 ***************************************************************************/
CAVERN_FORMAT_TYPE CavernDetectFormat(
    _In_reads_bytes_(BufferSize) PUCHAR Buffer,
    _In_ SIZE_T BufferSize
)
{
    return CavernDetectFormatInline(Buffer, BufferSize);
}

/**************************************************************************
 * CavernIsSyncWord
 ***************************************************************************/
BOOLEAN CavernIsSyncWord(
    _In_reads_bytes_(4) PUCHAR Buffer,
    _Out_ CAVERN_FORMAT_TYPE* Format
)
{
    if (Buffer[0] == 0x0B && Buffer[1] == 0x77) {
        *Format = CavernFormatAC3;
        return TRUE;
    }
    
    ULONG syncWord = ((ULONG)Buffer[0] << 24) | 
                     ((ULONG)Buffer[1] << 16) | 
                     ((ULONG)Buffer[2] << 8) | 
                     (ULONG)Buffer[3];
    
    if (syncWord == TRUEHD_SYNC_WORD_1 || syncWord == TRUEHD_SYNC_WORD_2) {
        *Format = CavernFormatTrueHD;
        return TRUE;
    }
    
    if (syncWord == DTS_SYNC_WORD) {
        *Format = CavernFormatDTS;
        return TRUE;
    }
    
    *Format = CavernFormatUnknown;
    return FALSE;
}

/**************************************************************************
 * CavernGetFormatInfo
 ***************************************************************************/
VOID CavernGetFormatInfo(
    _In_ CAVERN_FORMAT_TYPE Format,
    _Out_ PCAVERN_FORMAT_INFO Info
)
{
    RtlZeroMemory(Info, sizeof(CAVERN_FORMAT_INFO));
    
    Info->Format = Format;
    Info->IsPassthrough = TRUE;  // Default to passthrough
    
    switch (Format) {
        case CavernFormatPCM:
            Info->SampleRate = 48000;
            Info->Channels = 2;
            Info->BitRate = 1536000;
            Info->IsAtmos = FALSE;
            break;
            
        case CavernFormatAC3:
            Info->SampleRate = 48000;
            Info->Channels = 6;
            Info->BitRate = 640000;
            Info->IsAtmos = FALSE;
            break;
            
        case CavernFormatEAC3:
            Info->SampleRate = 48000;
            Info->Channels = 8;
            Info->BitRate = 768000;
            Info->IsAtmos = TRUE;  // Can carry Atmos
            break;
            
        case CavernFormatTrueHD:
            Info->SampleRate = 192000;
            Info->Channels = 8;
            Info->BitRate = 18000000;
            Info->IsAtmos = TRUE;
            break;
            
        case CavernFormatDTS:
            Info->SampleRate = 48000;
            Info->Channels = 6;
            Info->BitRate = 1536000;
            Info->IsAtmos = FALSE;
            break;
            
        case CavernFormatDTSHD:
            Info->SampleRate = 192000;
            Info->Channels = 8;
            Info->BitRate = 24500000;
            Info->IsAtmos = FALSE;
            break;
            
        default:
            Info->Format = CavernFormatUnknown;
            Info->IsPassthrough = FALSE;
            break;
    }
}
