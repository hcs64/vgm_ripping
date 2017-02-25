puts 'voxhound 0.3 by hcs'

def valid_frame?(frame, idx)
  # TODO: generally we have expectations and it would
  # be nice to check them:
  # 4 at the beginning of mono from MFAudio
  # 6 at the beginning of stereo from MFAudio
  # 7 at the end of a stream from MFAudio (after usable data)
  # 2 in the original VOX
  # 0 otherwise
  flags = frame[idx+1].ord
  (frame[idx+0].ord < 0x50 && (flags == 6 || flags == 4 || flags == 2 || flags == 0))
end

if ARGV[0] == '-r'
  mode = 'read'
  vox_fn = ARGV[1]
  vag_fn = ARGV[2]
  vox = File.open vox_fn, mode:'rb'
  vag = File.open vag_fn, mode:'wb'

  puts "extracting audio from #{vox_fn} to #{vag_fn}"
elsif ARGV[0] == '-w'
  mode = 'write'
  vox_fn = ARGV[1]
  vag_fn = ARGV[2]
  vox = File.open vox_fn, mode:'r+b'
  vag = File.open vag_fn, mode:'rb'

  puts "replacing audio in #{vox_fn} with #{vag_fn}"
else
  puts <<-HERE
usage:
  extract audio: voxhound -r original.vox output.vag
  replace audio: voxhound -w original.vox new_audio.vag

HERE
  exit
end

until vox.eof?
  # read block header
  offset = vox.tell
  header = vox.read(4).unpack('<L')[0]
  type = header & 0xFF
  size = header >> 8
  size >= 4 or raise '%08x: block size %x too small for header' % [offset, size]
  body_size = size - 4

  if type == 0xF0
    # end of file, only padding remains
    until vox.eof?
      vox.read(1).ord == 0 or raise 'nonzero padding after F0 block'
    end
  elsif type == 1
    # audio block
    body_size % 16 == 0 or raise 'body not evenly divided into frames'

    frames = body_size / 16

    if mode == 'read'
      body = vox.read body_size
      body != nil && body.length == body_size or raise 'short read'

      frames.times { |i|
        valid_frame? body, 16*i or raise '%08x: doesn\'t look like a valid frame' % (offset + 4 + 16*i)
      }

      vag.write body
    elsif mode == 'write'
      body = vag.read body_size
      body != nil && body.length == body_size or raise 'short read'

      frames.times { |i|
        valid_frame? body, 16*i or raise '%s %08x: doesn\'t look like a valid frame' % [vag_fn, vag.tell - body_size + 16*i]
        body[16*i+1] = 2.chr # force flags to 2
      }

      vox.write body
    end
  else
    # skip all other block types
    vox.seek body_size, IO::SEEK_CUR
  end

  # uncomment to show all blocks while parsing
  #puts '%08x: %02x %x' % [offset, type, body_size]
end

# currently ignore extra data when replacing, this is often just an extra
# frame with 07 flags indicating end of stream
#if mode == 'write'
#  vag.eof? or raise 'extra data in .vag'
#end

vag.close
vox.close

puts 'ok!'
puts
