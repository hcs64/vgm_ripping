lara 0.1

--------------------------------------------------------------------------------
Convert some files from "one of the Tomb Raider games (Legend/Anniversary)" to
standard Microsoft IMA ADPCM format .wav. The source files have a SECT header.

The script requires at least Ruby 1.9.3.

Really all it does is build a RIFF WAVE header.

The files I tested with end up clipped, so I think there is still something
missing.

---
Notes about making executables for Ruby:

Windows EXE generated with ocra 1.3.6 via
ocra --no-enc --console --gem-minimal --no-autodll --no-dep-run --verbose lara.rb

Name: ocra-1.3.6.gem
Size: 111104 bytes (0 MB)
SHA256: BB8FE3F1D3A4065E87003697A0F7CD60E56E7065D01756F4BC842633078ED6C1

My runtime was Ruby 2.2.5 from RubyInstaller:

Name: ruby-2.2.5-i386-mingw32.7z
Size: 9683305 bytes (9 MB)
SHA256: BE0DD3F56986FFAF4789A9978CA6D155ABF5407DB2A64E8ABD82C61035BFB274
