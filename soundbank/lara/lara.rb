for fn in ARGV do
  File.open(fn, mode='rb') do |f|
    print fn, ': '

    magic, data_size, six, zero1, id, neg1, sample_rate, zero2, zero3 =
      f.read(0x24).unpack('a4 L<4 l< L<3')

    raise 'Missing SECT' unless magic == 'SECT'
    raise 'unknown values differ' unless
      six == 6 and zero1 == 0 and neg1 == -1 and zero2 == 0 and zero3 == 0

    data_size -= 0xc
    print data_size, " bytes, ", sample_rate, "Hz\n"

    out_name = File.basename(fn, File.extname(fn)) + ".wav"
    raise "Output #{out_name} already exists" if File.exists?(out_name)

    File.open(out_name, mode='wb') do |out|
      codec_id = 0x11 # Microsoft IMA ADPCM
      channels = 1
      block_size = 0x24
      samples_per_block = (block_size - 4) * 2 + 1
      byte_rate = sample_rate * block_size / samples_per_block * channels
      sample_size = 4

      fmt = [codec_id, channels, sample_rate, byte_rate, block_size * channels, sample_size, 2, samples_per_block
        ].pack('S< S< L< L< S< S< S< S<')

      wave = ['WAVE', 'fmt ', fmt.size, fmt, 'data', data_size
        ].pack('a4 a4 L< a* a4 L<')

      out.write( ['RIFF', wave.size + data_size].pack('a4L<' ) )
      out.write( wave )
      
      copied = IO.copy_stream(f, out, data_size)
      raise 'data truncated' if copied != data_size

      raise 'extra data' unless f.eof
    end
  end
end
