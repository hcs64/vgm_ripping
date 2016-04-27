// from xact3wb.h

#define ADPCM_MINIWAVEFORMAT_BLOCKALIGN_CONVERSION_OFFSET 22

#define WAVEBANK_HEADER_SIGNATURE               'DNBW'      // WaveBank  RIFF chunk signature
#define WAVEBANK_HEADER_VERSION                 43          // Current wavebank file version

#define WAVEBANK_BANKNAME_LENGTH                64          // Wave bank friendly name length, in characters
#define WAVEBANK_ENTRYNAME_LENGTH               64          // Wave bank entry friendly name length, in characters

#define WAVEBANK_MAX_DATA_SEGMENT_SIZE          0xFFFFFFFF  // Maximum wave bank data segment size, in bytes
#define WAVEBANK_MAX_COMPACT_DATA_SEGMENT_SIZE  0x001FFFFF  // Maximum compact wave bank data segment size, in bytes

typedef uint32_t WAVEBANKOFFSET;

//
// Bank flags
//

#define WAVEBANK_TYPE_BUFFER         0x00000000      // In-memory buffer
#define WAVEBANK_TYPE_STREAMING      0x00000001      // Streaming
#define WAVEBANK_TYPE_MASK           0x00000001

#define WAVEBANK_FLAGS_ENTRYNAMES    0x00010000      // Bank includes entry names
#define WAVEBANK_FLAGS_COMPACT       0x00020000      // Bank uses compact format
#define WAVEBANK_FLAGS_SYNC_DISABLED 0x00040000      // Bank is disabled for audition sync
#define WAVEBANK_FLAGS_SEEKTABLES    0x00080000      // Bank includes seek tables.
#define WAVEBANK_FLAGS_MASK          0x000F0000

//
// Entry flags
//

#define WAVEBANKENTRY_FLAGS_READAHEAD       0x00000001  // Enable stream read-ahead
#define WAVEBANKENTRY_FLAGS_LOOPCACHE       0x00000002  // One or more looping sounds use this wave
#define WAVEBANKENTRY_FLAGS_REMOVELOOPTAIL  0x00000004  // Remove data after the end of the loop region
#define WAVEBANKENTRY_FLAGS_IGNORELOOP      0x00000008  // Used internally when the loop region can't be used
#define WAVEBANKENTRY_FLAGS_MASK            0x00000008

//
// Entry wave format identifiers
//

#define WAVEBANKMINIFORMAT_TAG_PCM      0x0     // PCM data
#define WAVEBANKMINIFORMAT_TAG_XMA      0x1     // XMA data
#define WAVEBANKMINIFORMAT_TAG_ADPCM    0x2     // ADPCM data
#define WAVEBANKMINIFORMAT_TAG_WMA      0x3     // WMA data

#define WAVEBANKMINIFORMAT_BITDEPTH_8   0x0     // 8-bit data (PCM only)
#define WAVEBANKMINIFORMAT_BITDEPTH_16  0x1     // 16-bit data (PCM only)

//
// Arbitrary fixed sizes
//
#define WAVEBANKENTRY_XMASTREAMS_MAX          3   // enough for 5.1 channel audio
#define WAVEBANKENTRY_XMACHANNELS_MAX         6   // enough for 5.1 channel audio (cf. XAUDIOCHANNEL_SOURCEMAX)

//
// DVD data sizes
//

#define WAVEBANK_DVD_SECTOR_SIZE    2048
#define WAVEBANK_DVD_BLOCK_SIZE     (WAVEBANK_DVD_SECTOR_SIZE * 16)

//
// Bank alignment presets
//

#define WAVEBANK_ALIGNMENT_MIN  4                           // Minimum alignment
#define WAVEBANK_ALIGNMENT_DVD  WAVEBANK_DVD_SECTOR_SIZE    // DVD-optimized alignment

//
// Wave bank segment identifiers
//

typedef enum WAVEBANKSEGIDX {
    WAVEBANK_SEGIDX_BANKDATA = 0,       // Bank data
    WAVEBANK_SEGIDX_ENTRYMETADATA,      // Entry meta-data
    WAVEBANK_SEGIDX_SEEKTABLES,         // Storage for seek tables for the encoded waves.
    WAVEBANK_SEGIDX_ENTRYNAMES,         // Entry friendly names
    WAVEBANK_SEGIDX_ENTRYWAVEDATA,      // Entry wave data
    WAVEBANK_SEGIDX_COUNT
} WAVEBANKSEGIDX, *LPWAVEBANKSEGIDX;

#pragma pack(1)

typedef struct {
    uint32_t    dwOffset;                               // Region offset, in bytes
    uint32_t    dwLength;                               // Region length, in bytes
} WAVEBANKREGION;

typedef struct {
    char            dwSignature[4];                     // (uint32_t -> char[4]) File signature
    uint32_t        dwVersion;                          // Version of the tool that created the file
    WAVEBANKREGION  Segments[WAVEBANK_SEGIDX_COUNT];    // Segment lookup table
} WAVEBANKHEADER;

typedef struct {
    uint32_t    dwStartSample;          // Start sample for the region.
    uint32_t    dwTotalSamples;         // Region length in samples.
} WAVEBANKSAMPLEREGION;

typedef struct {
    uint32_t        dwFlagsAndDuration;                 // dwFlags:4 and Duration:28
    uint32_t        Format;                             // Entry format
    WAVEBANKREGION  PlayRegion;                         // Region within the wave data segment that contains this entry
    union {
        WAVEBANKREGION          LoopRegion; // Region within the wave data that should loop

        // XMA loop region
        // Note: this is not the same memory layout as the XMA loop region
        // passed to the XMA driver--it is more compact. The named values
        // map correctly and there are enough bits to store the entire
        // range of values that XMA considers valid, with one exception:
        // valid values for nSubframeSkip are 1, 2, 3, or 4. In order to
        // store this in two bits, XACT subtracts 1 from the value, then adds

        struct
        {
            uint32_t    dwStartOffset;          // loop start offset (in bits)
            uint32_t    nSubframeSkip_nSubframeEnd_dwEndOffset;
            //uint32_t    nSubframeSkip   : 2;    // needed by XMA decoder. Valid values for XMA are 1-4; XACT converts to 0-3 for storage. Add 1 to this value before passing to driver.
            //uint32_t    nSubframeEnd    : 2;    // needed by XMA decoder
            //uint32_t    dwEndOffset     : 28;   // loop end offset (in bits)
        } XMALoopRegion[ WAVEBANKENTRY_XMASTREAMS_MAX ];

        // The last element in the union is an array that aliases the
        // entire union so we can byte-reverse the whole thing.
        WAVEBANKREGION LoopRegionAlias[ WAVEBANKENTRY_XMASTREAMS_MAX ];
    };
} WAVEBANKENTRY;

typedef struct {
    uint32_t    dwFlags;                                // Bank flags
    uint32_t    dwEntryCount;                           // Number of entries in the bank
    char        szBankName[WAVEBANK_BANKNAME_LENGTH];   // Bank friendly name
    uint32_t    dwEntryMetaDataElementSize;             // Size of each entry meta-data element, in bytes
    uint32_t    dwEntryNameElementSize;                 // Size of each entry name element, in bytes
    uint32_t    dwAlignment;                            // Entry alignment, in bytes
    uint32_t    CompactFormat;                          // Format data for compact bank
    uint32_t    BuildTime;                              // Build timestamp
} WAVEBANKDATA;

#pragma pack()
