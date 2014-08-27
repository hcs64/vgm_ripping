#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <png.h>

uint32_t read_32bitBE(uint8_t *c)
{
    uint32_t v = 0;
    for (int i = 0; i < 4; i++)
    {
        v *= 0x100;
        v += c[i];
    }
    return v;
}

uint16_t read_16bitBE(uint8_t *c)
{
    uint16_t v = c[0];
    v *= 0x100;
    v += c[1];

    return v;
}

uint32_t read_32bitLE(uint8_t *c)
{
    uint32_t v = 0;
    for (int i = 4-1; i >= 0; i--)
    {
        v *= 0x100;
        v += c[i];
    }
    return v;
}

void render_frame(const char *filename, FILE *infile, int set, int frame, long gtex_offset, int pal_idx, int tex_idx, int gfo_type);
void process_texture(const char *filename, FILE *infile, long palette_offset, long texture_offset, int type);
void output_png(const char *filename, const uint8_t *tex, const uint8_t *pal, unsigned int line_width, unsigned int lines, int bpp, int indexed);

int main(int argc, char ** argv)
{
    int gfo_type = -1;
    long gtex_offset = -1;
    long gtpa_offset = -1;

    if (argc != 2)
    {
        printf("gfo2png 0.2\n");
        printf("usage: %s input.gfo3\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    FILE *infile = fopen(argv[1], "rb");
    if (!infile)
    {
        fprintf(stderr, "error opening %s\n", argv[1]);
        goto fail;
    }

    // guess type from file name
    {
        const char *ext = strrchr(argv[1],'.');
        if (ext)
        {
            if (!strcmp(ext, ".gfo2"))
            {
                gfo_type = 2;
            }
            else if (!strcmp(ext, ".gfo3"))
            {
                gfo_type = 3;
            }
        }

        if (-1 == gfo_type)
        {
            printf("Couldn't guess GFOF type from extension, assuming 3\n");
            gfo_type = 3;
        }
    }

    // locate GTPA and GTEX chunks
    {
        uint8_t chunkbuf[4];
        while (4==fread(chunkbuf, 1, 4, infile))
        {
            if (!memcmp(chunkbuf,"GTEX",4))
            {
                gtex_offset = ftell(infile) + 4;
            }
            if (!memcmp(chunkbuf,"GTPA",4))
            {
                gtpa_offset = ftell(infile) + 4;
            }

            if (4 != fread(chunkbuf, 1, 4, infile)) goto fail;
            if (read_32bitBE(chunkbuf) < 8) goto fail;
            if (0 != fseek(infile, read_32bitBE(chunkbuf)-8, SEEK_CUR)) goto fail;
        }

        if (-1 == gtex_offset || -1 == gtpa_offset)
        {
            fprintf(stderr, "GTPA and GTEX chunks not found\n");
            goto fail;
        }
    }

    // collect textures
    {
        uint8_t buf[0x14];

        // read GTEX
        int pal_count, tex_count;
        if (0 != fseek(infile, gtex_offset, SEEK_SET)) goto fail;
        if (4 != fread(buf, 1, 4, infile)) goto fail;

        pal_count = read_16bitBE(&buf[0]);
        tex_count = read_16bitBE(&buf[2]);

        printf("%d palette%s, %d texture%s\n",
            pal_count, (pal_count==1?"":"s"), tex_count, tex_count==1?"":"s");

        // read GTPA
        int texture_sets; // animations?
        if (0 != fseek(infile, gtpa_offset, SEEK_SET)) goto fail;
        if (0xc != fread(buf, 1, 0xc, infile)) goto fail;

        texture_sets = read_32bitBE(&buf[8]);
        printf("%d texture set%s\n", texture_sets, (texture_sets==1?"":"s"));

        for (int i = 0; i < texture_sets; i++)
        {
            long gtpd_offset;
            long gtpd_list_offset;

            if (0 != fseek(infile, gtpa_offset + 0xc + i*4, SEEK_SET)) goto fail;
            if (4 != fread(buf, 1, 4, infile)) goto fail;

            gtpd_offset = read_32bitBE(&buf[0]);

            // read GTPD
            if (2 == gfo_type)
            {
                // missing an unknown dword
                gtpd_list_offset = 0x10;
            }
            else if (3 == gfo_type)
            {
                gtpd_list_offset = 0x14;
            }
            else goto fail;

            if (0 != fseek(infile, gtpd_offset, SEEK_SET)) goto fail;
            if (gtpd_list_offset != fread(buf, 1, gtpd_list_offset, infile)) goto fail;

            if (memcmp(&buf[0],"GTPD",4)) goto fail;
            int palette_keyframe_count = read_16bitBE(&buf[gtpd_list_offset-4]);
            int texture_keyframe_count = read_16bitBE(&buf[gtpd_list_offset-2]);

            struct {
                uint16_t frames;
                uint16_t idx;
            } *palette_keyframes, *texture_keyframes;
            uint16_t last_frame = 0;

            palette_keyframes = malloc(sizeof(*palette_keyframes)*palette_keyframe_count);
            texture_keyframes = malloc(sizeof(*texture_keyframes)*texture_keyframe_count);

            if (!palette_keyframes || !texture_keyframes) goto fail;

            if (0 != fseek(infile, gtpd_offset + gtpd_list_offset, SEEK_SET)) goto fail;
            for (int j = 0; j < palette_keyframe_count; j++)
            {
                if (4 != fread(buf, 1, 4, infile)) goto fail;
                palette_keyframes[j].frames = read_16bitBE(&buf[0]);
                palette_keyframes[j].idx = read_16bitBE(&buf[2]);

                if (palette_keyframes[j].frames > last_frame)
                {
                    last_frame = palette_keyframes[j].frames;
                }

                if (palette_keyframes[j].idx >= pal_count) goto fail;
            }
            for (int j = 0; j < texture_keyframe_count; j++)
            {
                if (4 != fread(buf, 1, 4, infile)) goto fail;
                texture_keyframes[j].frames = read_16bitBE(&buf[0]);
                texture_keyframes[j].idx = read_16bitBE(&buf[2]);

                if (texture_keyframes[j].frames > last_frame)
                {
                    last_frame = texture_keyframes[j].frames;
                }

                if (texture_keyframes[j].idx >= tex_count) goto fail;
            }

            // frame by frame, only outputting when palette or texture changes
            int last_palette_idx = -1, last_texture_idx = -1;
            int next_palette_keyframe_idx = 0, next_texture_keyframe_idx = 0;

            for (int frame = 0; frame <= last_frame; frame ++)
            {
                int palette_idx = last_palette_idx, texture_idx = last_texture_idx;
                if (next_palette_keyframe_idx < palette_keyframe_count &&
                    palette_keyframes[next_palette_keyframe_idx].frames == frame)
                {
                    palette_idx = palette_keyframes[next_palette_keyframe_idx].idx;
                    next_palette_keyframe_idx ++;
                }
                if (next_texture_keyframe_idx < texture_keyframe_count &&
                    texture_keyframes[next_texture_keyframe_idx].frames == frame)
                {
                    texture_idx = texture_keyframes[next_texture_keyframe_idx].idx;
                    next_texture_keyframe_idx ++;
                }

                if (palette_idx != last_palette_idx ||
                    texture_idx != last_texture_idx)
                {
                    if (-1 == texture_idx &&
                        texture_keyframe_count > 0)
                    {
                        texture_idx = texture_keyframes[0].idx;
                    }
                    if (-1 == palette_idx &&
                        palette_keyframe_count > 0)
                    {
                        palette_idx = palette_keyframes[0].idx;
                    }

                    printf("%d: frame %d: palette %d, texture %d\n", i, frame, palette_idx, texture_idx);

                    
                    render_frame(argv[1], infile, i, frame, gtex_offset, palette_idx, pal_count + texture_idx, gfo_type);
                }

                last_palette_idx = palette_idx;
                last_texture_idx = texture_idx;
            }

            free(palette_keyframes);
            free(texture_keyframes);
        }
    }

    fclose(infile);
    printf("Done!\n");

    return 0;
fail:
    fprintf(stderr, "failed.\n");
    exit(EXIT_FAILURE);
}

void render_frame(const char *filename, FILE *infile, int set, int frame, long gtex_offset, int pal_idx, int tex_idx, int gfo_type)
{
    char buf[4];
    size_t namelen = strlen(filename) + 1 + 2 + 1 + 3 + 4 + 10 + 1;
    char *outname = malloc(namelen);
    if (!outname) goto fail;
    snprintf(outname, namelen, "%s_%02d_%03d.png", filename, set, frame);

    long pal_offset;
    long tex_offset;

    if (-1 != pal_idx)
    {
        if (0 != fseek(infile, gtex_offset + 4 + pal_idx * 4, SEEK_SET)) goto fail;
        if (4 != fread(buf, 1, 4, infile)) goto fail;
        pal_offset = read_32bitBE(buf);
    }
    else
    {
        pal_offset = -1;
    }

    if (0 != fseek(infile, gtex_offset + 4 + tex_idx * 4, SEEK_SET)) goto fail;
    if (4 != fread(buf, 1, 4, infile)) goto fail;
    tex_offset = read_32bitBE(buf);

    process_texture(outname, infile, pal_offset, tex_offset, gfo_type);

    free(outname);

    return;
fail:

    fprintf(stderr, "failed rendering set %d frame %d\n", set, frame);
    exit(EXIT_FAILURE);
}

// unlz77wii_raw30 from DSDecmp: http://code.google.com/p/dsdecmp/
// ported to C by Luigi Auriemma
int unlz77wii_raw30(const uint8_t *in, long insz, uint8_t *outdata, long decomp_size) {
    long         curr_size = 0;
    const uint8_t *inl = in + insz;

    while (in < inl)
    {
        uint8_t flag;
        int rl;
        uint8_t compressed;

        // get tag
        flag = *in++;
        compressed = (flag & 0x80);
        rl = flag & 0x7F;
        if (compressed)
            rl += 3;
        else
            rl += 1;
        if (compressed)
        {
            uint8_t b;
            if(in >= inl) goto fail;
            if(curr_size+rl > decomp_size) goto fail;
            b = *in++;
            for (int i = 0; i < rl; i++)
            {
                outdata[curr_size++] = b;
            }
        }
        else
        {
            if(in >= inl) goto fail;
            if(curr_size+rl > decomp_size) goto fail;
            for (int i = 0; i < rl; i++) {
                outdata[curr_size++] = *in++;
            }
        }
    }

    if (curr_size != decomp_size || in != inl)
    {
        fprintf(stderr,"%lx != %lx\n",(unsigned long)curr_size,(unsigned long)decomp_size);
        goto fail;
    }

    return(curr_size);

fail:
    fprintf(stderr,"decompression did not go as expected\n");
    exit(EXIT_FAILURE);
}

void process_texture(const char *filename, FILE *infile, long palette_offset, long texture_offset, int type)
{
    uint8_t buf[0x14];
    uint8_t palette_buf[0x200];
    int bpp = 8;
    int indexed = 1;
    
    // read texture header
    if (0 != fseek(infile, texture_offset, SEEK_SET)) goto fail;
    if (0x4 != fread(buf, 1, 0x4, infile)) goto fail;
    int width = read_16bitBE(&buf[0]);
    int height = read_16bitBE(&buf[2]);
    if (3 == type)
    {
        if (0x4 != fread(buf, 1, 0x4, infile)) goto fail;
        switch (read_32bitBE(&buf[0]))
        {
            case 1:
                bpp = 8;
                indexed = 1;
                break;
            case 2:
                bpp = 4;
                indexed = 1;
                break;
            // pretty unclear on what to do in this case
            case 4:
                bpp = 4;
                indexed = 0;
                break;
            default:
                goto fail;
        }
    }
    else if (2 == type)
    {
        // No extra dword
    } else goto fail;

    if (0x4 != fread(buf, 1, 0x4, infile)) goto fail;
    uint32_t texture_size = read_32bitBE(&buf[0]);

    // read GFCP header
    if (0x14 != fread(buf, 1, 0x14, infile)) goto fail;
    if (memcmp(&buf[0], "GFCP", 4)) goto fail;
    if (1 != read_32bitLE(&buf[4])) goto fail;
    if (2 != read_32bitLE(&buf[8])) goto fail; // compression type = RLE
    uint32_t data_size = read_32bitLE(&buf[0xc]);
    uint32_t compressed_data_size = read_32bitLE(&buf[0x10]);
    if (texture_size-0x14 != compressed_data_size) goto fail;

    // read in, decompress texture
    uint8_t *compressed_texture_buf = malloc(compressed_data_size);
    uint8_t *texture_buf = malloc(data_size);
    if (!compressed_texture_buf || !texture_buf) goto fail;
    if (compressed_data_size != fread(compressed_texture_buf, 1, compressed_data_size, infile)) goto fail;
    unlz77wii_raw30(compressed_texture_buf, compressed_data_size, texture_buf, data_size);
    free(compressed_texture_buf);

    if (indexed)
    {
        if (-1 == palette_offset) goto fail;
        // read in palette header
        if (0 != fseek(infile, palette_offset, SEEK_SET)) goto fail;
        if (3 == type)
        {
            if (4 != fread(buf, 1, 4, infile)) goto fail;

            switch (buf[0])
            {
                case 0:
                    if (bpp != 8) goto fail;
                    break;
                case 1:
                    if (bpp != 4) goto fail;
                    break;
                default:
                    goto fail;
            }
        }
        else if (2 == type)
        {
            // No extra dword
        }
        else goto fail;
        // read in palette
        if ((2 << bpp) != fread(palette_buf, 1, (2 << bpp), infile)) goto fail;
    }

    // write PNG
    output_png(filename, texture_buf, palette_buf, width, height, bpp, indexed);

    free(texture_buf);

    return;

fail:
    fprintf(stderr, "failed processing texture\n");
    exit(EXIT_FAILURE);
}

void user_warning_fn(png_structp png, png_const_charp msg)
{
    fprintf(stderr, "PNG Warning: %s\n", msg);
}
void user_error_fn(png_structp png, png_const_charp msg)
{
    fprintf(stderr, "PNG Error: %s\n", msg);
}

void output_png(const char *out_filename, const uint8_t *tex, const uint8_t *pal, unsigned int line_width, unsigned int lines, int bpp, int indexed)
{
    const unsigned int tile_width = 8;
    const unsigned int tile_height = 32/tile_width*8/bpp;

    unsigned int real_line_width = line_width;
    line_width = (line_width+tile_width-1)/tile_width*tile_width;

    uint8_t *linebuf = malloc(line_width*
            ((lines+tile_height-1)/tile_height*tile_height)*4);
    uint8_t **line_pointers = malloc(sizeof(uint8_t*)*lines);

    FILE *outfile = fopen(out_filename, "wb");

    if (!outfile || !tex || !pal || !linebuf || !line_pointers) goto fail;

    printf("%s: %dx%d %d bpp %s\n", out_filename, real_line_width, lines, bpp,
        (indexed?"indexed":""));

    // Set Up PNG
    png_structp png_ptr = png_create_write_struct
        (PNG_LIBPNG_VER_STRING, (png_voidp)NULL, user_error_fn, user_warning_fn);
    if (!png_ptr) goto fail;

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) goto fail;

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        goto fail;
    }

    png_init_io(png_ptr, outfile);

    png_set_IHDR(png_ptr, info_ptr, real_line_width, lines,
        8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    // set up palette
    uint8_t palette[256][4];
    if (indexed)
    {
        for (unsigned int i = 0; i < (1 << bpp); i++)
        {
            const uint8_t *col = &pal[i*2];

            // RGB5A3, figured out thanks to tpu's brres source
            if (col[0] & 0x80)
            {
                palette[i][0] = (col[0]&0x7C)<<1;
                palette[i][1] = ((col[0]&0x3)<<6)|((col[1]&0xe0)>>2);
                palette[i][2] = ((col[1]&0x1f)<<3);
                palette[i][3] = 0xff;
            }
            else
            {
                palette[i][0] = ((col[1]&0xf)<<4);
                palette[i][1] = ((col[1]&0xf0));
                palette[i][2] = ((col[0]&0xf)<<4);
                palette[i][3] = (col[0]&0x70)<<1;
            }
        }
    }

    // detile and lookup
    for (unsigned int line = 0; line < lines; line += tile_height)
    {
        for (unsigned int col = 0; col < line_width; col += tile_width)
        {
            for (unsigned int y = 0; y < tile_height; y++)
            {
                for (unsigned int x = 0; x < tile_width; x++)
                {
                    uint8_t * const dest_pixel = &linebuf[((line+y)*line_width+col+x)*4];
                    uint8_t src_pixel = tex[(line*line_width+col*tile_height+y*tile_width+x)*bpp/8];

                    if (bpp == 4)
                    {
                        if (x & 1)
                        {
                            src_pixel &= 0xf;
                        }
                        else
                        {
                            src_pixel >>= 4;
                        }
                    }

                    if (indexed)
                    {
                        dest_pixel[0] = palette[src_pixel][0];
                        dest_pixel[1] = palette[src_pixel][1];
                        dest_pixel[2] = palette[src_pixel][2];
                        dest_pixel[3] = palette[src_pixel][3];
                    }
                    else
                    {
#if 0
                        // RGB5A3, figured out thanks to tpu's brres source
                        uint8_t src_pixel2 = tex[(line*line_width+col*tile_height+y*tile_width+x)*bpp/8+1];
                        if (src_pixel & 0x80)
                        {
                            dest_pixel[0] = (src_pixel&0x7C)<<1;
                            dest_pixel[1] = ((src_pixel&0x3)<<6)|((src_pixel2&0xe0)>>2);
                            dest_pixel[2] = ((src_pixel2&0x1f)<<3);
                            dest_pixel[3] = 0xff;
                        }
                        else
                        {
                            dest_pixel[0] = ((src_pixel2&0xf)<<4);
                            dest_pixel[1] = ((src_pixel2&0xf0));
                            dest_pixel[2] = ((src_pixel&0xf)<<4);
                            dest_pixel[3] = (src_pixel&0x70)<<1;
                        }
#endif
                        if (bpp == 4)
                        {
                            src_pixel <<= 4;
                        }

                        dest_pixel[0] = src_pixel;
                        dest_pixel[1] = src_pixel;
                        dest_pixel[2] = src_pixel;
                        dest_pixel[3] = 0xff;
                    }
                }
            }
        }
    }

    // Do PNG
    for (unsigned int line = 0; line < lines; line ++)
    {
        line_pointers[line] = &linebuf[line*line_width*4];
    }
    png_set_rows(png_ptr, info_ptr, line_pointers);
    png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

    png_destroy_write_struct(&png_ptr, &info_ptr);
    if (0 != fclose(outfile)) goto fail;

    free(linebuf);
    free(line_pointers);

    return;
fail:
    fprintf(stderr, "failed while trying to generate PNG.\n");
    exit(EXIT_FAILURE);
}
