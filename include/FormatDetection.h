/***************************************************************************
 * FormatDetection.h
 * 
 * Dolby Atmos format detection for CavernAudioDriver
 ***************************************************************************/

#pragma once

#include <ntddk.h>

// Format sync word definitions
#define AC3_SYNC_WORD           0x0B77      // Dolby Digital (AC3)
#define EAC3_SYNC_WORD          0x0B77      // Dolby Digital Plus (E-AC3)
#define TRUEHD_SYNC_WORD_1      0xF8726FBA  // Dolby TrueHD sync word 1
#define TRUEHD_SYNC_WORD_2      0x72FBA870  // Dolby TrueHD sync word 2
#define DTS_SYNC_WORD           0x7FFE8001  // DTS sync word
#define DTS_HD_SYNC_WORD        0x64582025  // DTS-HD sync word

// Format detection results
typedef enum _CAVERN_FORMAT_TYPE {
    CavernFormatUnknown = 0,
    CavernFormatPCM,
    CavernFormatAC3,
    CavernFormatEAC3,
    CavernFormatTrueHD,
    CavernFormatDTS,
    CavernFormatDTSHD,
    CavernFormatMAT        // Dolby MAT (Atmos)
} CAVERN_FORMAT_TYPE;

// Format information
typedef struct _CAVERN_FORMAT_INFO {
    CAVERN_FORMAT_TYPE Format;
    ULONG SampleRate;
    ULONG Channels;
    ULONG BitRate;
    BOOLEAN IsAtmos;
    BOOLEAN IsPassthrough;
} CAVERN_FORMAT_INFO, *PCAVERN_FORMAT_INFO;

// Function prototypes
CAVERN_FORMAT_TYPE CavernDetectFormat(
    _In_reads_bytes_(BufferSize) PUCHAR Buffer,
    _In_ SIZE_T BufferSize
);

BOOLEAN CavernIsSyncWord(
    _In_reads_bytes_(4) PUCHAR Buffer,
    _Out_ CAVERN_FORMAT_TYPE* Format
);

VOID CavernGetFormatInfo(
    _In_ CAVERN_FORMAT_TYPE Format,
    _Out_ PCAVERN_FORMAT_INFO Info
);

// Format detection implementation
FORCEINLINE
CAVERN_FORMAT_TYPE CavernDetectFormatInline(
    _In_reads_bytes_(BufferSize) PUCHAR Buffer,
    _In_ SIZE_T BufferSize
)
{
    if (BufferSize < 4) {
        return CavernFormatUnknown;
    }
    
    // Check for AC3/E-AC3 (0x0B77)
    if (Buffer[0] == 0x0B && Buffer[1] == 0x77) {
        // Distinguish AC3 from E-AC3 by checking frame size
        // E-AC3 has different bitstream structure
        if (BufferSize >= 5) {
            UCHAR strmtyp = (Buffer[2] >> 5) & 0x3;
            if (strmtyp == 0 || strmtyp == 2) {
                return CavernFormatEAC3;
            }
        }
        return CavernFormatAC3;
    }
    
    // Check for TrueHD (4-byte sync word: 0xF8726FBA)
    if (BufferSize >= 4) {
        ULONG syncWord = ((ULONG)Buffer[0] << 24) | 
                         ((ULONG)Buffer[1] << 16) | 
                         ((ULONG)Buffer[2] << 8) | 
                         (ULONG)Buffer[3];
        
        if (syncWord == TRUEHD_SYNC_WORD_1 || syncWord == TRUEHD_SYNC_WORD_2) {
            return CavernFormatTrueHD;
        }
    }
    
    // Check for DTS (0x7FFE8001)
    if (BufferSize >= 4) {
        ULONG syncWord = ((ULONG)Buffer[0] << 24) | 
                         ((ULONG)Buffer[1] << 16) | 
                         ((ULONG)Buffer[2] << 8) | 
                         (ULONG)Buffer[3];
        
        if (syncWord == DTS_SYNC_WORD) {
            return CavernFormatDTS;
        }
    }
    
    // Check for PCM (look for common PCM patterns)
    // PCM typically doesn't have specific sync words, but we can detect
    // by absence of compressed format markers
    
    return CavernFormatUnknown;
}
