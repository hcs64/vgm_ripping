require "chunky_png"

fn = ARGV[0]
exit unless fn
File.open(fn, mode='rb') do |f|
  puts fn

  width = ARGV[1].to_i

  raise 'bad width' if width == 0

  magic, unk1, subfiles = (f.read(16).unpack('a8 L<2'))
  raise 'Missing DDWLG00' unless magic == "DDWLG00\x00"

  puts "#{subfiles} images"

  start = 0x60
  f.seek(start, IO::SEEK_SET)

  subfiles.times do |n|
    out_name = "#{File.basename(fn, File.extname(fn))}_#{n}.png"

    fmt, = f.read(2).unpack('S<')
    f.seek(0x3e, IO::SEEK_CUR)

    packed_size, = f.read(4).unpack('L<')

    unpacked_size = 0

    StringIO.open('', 'w+b') do |out_data|
      while true
        header, = f.read(4).unpack('L<')
        if header == 0 then
          break
        end

        if header >= 0x80000000 then
          zeroes = header - 0x80000000
          zeroes.times do
            out_data.write "\x00\x00\x00\x00"
          end
          unpacked_size += zeroes * 4
        else
          count = header
          count.times do
            b,g,r,a = f.read(4).unpack('C4')
            out_data.write [r,g,b,a].pack('C4')
          end
          unpacked_size += count * 4
        end
      end

      height = unpacked_size / 4 / width
      print "#{out_name}:"
      print " #{f.tell.to_s(16)} #{fmt} #{packed_size} -> #{unpacked_size}"
      puts " #{width}x#{height}"
      out_data.seek(0, IO::SEEK_SET)
      image = ChunkyPNG::Canvas.from_rgba_stream(width, height, out_data)
      image.save(out_name)
    end
  end
end