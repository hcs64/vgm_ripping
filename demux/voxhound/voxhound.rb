def valid_frame?(frame, idx)
  flags = frame[idx+1].ord
  (frame[idx+0].ord < 0x50 and (flags == 4 or flags == 2 or flags == 0))
end

puts 'voxhound 0.2 by hcs'

if ARGV[0] == '-r'
  mode = 'read'
  vox_fn = ARGV[1]
  vag_fn = ARGV[2]
  vox = File.open(vox_fn, mode:'rb')
  vag = File.open(vag_fn, mode:'wb')

  puts "extracting audio from #{vox_fn} to #{vag_fn}"
elsif ARGV[0] == '-w'
  mode = 'write'
  vox_fn = ARGV[1]
  vag_fn = ARGV[2]
  vox = File.open(vox_fn, mode:'r+b')
  vag = File.open(vag_fn, mode:'rb')

  puts "replacing audio in #{vox_fn} with #{vag_fn}"
else
  puts <<-HERE
usage:
  extract audio: voxhound -r original.vox output.vag
  replace audio: voxhound -w original.vox new_audio.vag

HERE
  exit
end

while !vox.eof?
  # read block header
  offset = vox.tell
  header = vox.read(4).unpack('<L')[0]
  type = header & 0xFF
  size = header >> 8
  raise '%08x: block size %x too small for header' % [offset, size] if size < 4
  body_size = size - 4

  if type == 1
    # audio block
    if body_size % 16 != 0
      raise 'body not evenly divided into frames'
    end

    frames = body_size / 16

    if mode == 'read'
      body = vox.read(body_size)
      raise 'short read' if body == nil or body.length != body_size

      frames.times { |i|
        raise '%08x: doesn\'t look like a valid frame' % (offset + 4 + 16*i) if not valid_frame?(body, 16*i)
      }

      vag.write(body)
    elsif mode == 'write'
      body = vag.read(body_size)
      raise 'short read' if body == nil or body.length != body_size

      frames.times { |i|
        raise '%s %08x: doesn\'t look like a valid frame' % [vag_fn, vag.tell - body_size + 16*i] if not valid_frame?(body, 16*i)
        body[16*i+1] = 2.chr # force flags to 2

      }

      vox.write(body)
    end
  else # type != 1
    # skip all other block types
    vox.seek(body_size, IO::SEEK_CUR)
  end

  #puts '%08x: %02x %x' % [offset, type, body_size]
end

#if mode == 'write'
#  raise 'extra data in .vag' unless vag.eof?
#end

vag.close
vox.close

puts "ok!\n"
