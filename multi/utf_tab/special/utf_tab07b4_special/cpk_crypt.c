#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#include "utf_tab.h"
#include "error_stuff.h"
#include "util.h"

void analyze(reader_t *infile, long offset, long file_length);

int main(int argc, char **argv)
{
    printf("cpk_crypt " VERSION "\n\n");
    CHECK_ERROR(argc != 2, "Incorrect program usage\n\nusage: cpk_crypt file.cpk");

    /* open file */
    reader_t *infile = open_reader_file(argv[1]);
    CHECK_ERRNO(!infile, "fopen");

    long file_length = reader_length(infile);

    analyze(infile, 0, file_length);

    exit(EXIT_SUCCESS);
}

static const char UTF_signature[4] = "@UTF"; /* intentionally unterminated */

void analyze(reader_t *infile, long offset, long file_length)
{
    const long CpkHeader_offset = offset;

    /* check header */
    {
        unsigned char buf[4];
        static const char CPK_signature[4] = "CPK "; /* intentionally unterminated */
        get_bytes_seek(CpkHeader_offset, infile, buf, 4);
        CHECK_ERROR (memcmp(buf, CPK_signature, sizeof(CPK_signature)), "CPK signature not found");
    }

    const long CpkHeader_size = get_32_le_seek(CpkHeader_offset+8, infile);
    /* check CpkHeader */
    {
        enum {bytes_to_check=0x18};

        unsigned char buf[bytes_to_check];
        get_bytes_seek(CpkHeader_offset+0x10, infile, buf, bytes_to_check);
        CHECK_ERROR (!memcmp(buf, UTF_signature, sizeof(UTF_signature)), "@UTF table looks unencrypted");

        unsigned char expected_bytes[bytes_to_check];
        // signature
        memcpy(&expected_bytes[0], UTF_signature, 4);
        // CPK chunk size
        write_32_be(CpkHeader_size-8, &expected_bytes[4]);
        // CPK table name string offset (7, first in table after "<NULL>\0")
        write_32_be(7, &expected_bytes[0x14]);

        uint8_t xor_bytes[bytes_to_check];

        for (int i=0; i < bytes_to_check; i++) {
            xor_bytes[i]=expected_bytes[i]^buf[i];
        }

        for (int mult=0; mult <= UINT8_MAX; mult++) {
            uint8_t xor = xor_bytes[0];
            for (int i=1; i < bytes_to_check; i++) {
                xor *= mult;

                if ((i >= 0 && i < 8) ||
                    (i >= 0x14 && i < 0x18)) {
                    if (xor != xor_bytes[i]) break;
                }

                if (i == bytes_to_check-1) {
                    printf("s=%02"PRIx8" m=%02x\n",xor_bytes[0], (unsigned int)mult);
                }
            }
        }
    }
}
