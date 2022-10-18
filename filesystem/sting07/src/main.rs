use std::error::Error;
use std::fs::OpenOptions;
use std::io::{BufReader, Read, Write};
use std::path::PathBuf;

use lazy_static::lazy_static;
use std::collections::HashMap;

use flate2::read::ZlibDecoder;
use murmurhash64::murmur_hash64a;
use scroll::{Endian, IOread};

// with some reference to https://github.com/Atvaark/VermintideBundleTool

fn main() {
    if let Err(e) = fallible_main_dump() {
        eprintln!("{}", e);
    }
}

fn read32(f: &mut impl Read) -> std::io::Result<u32> {
    f.ioread_with(Endian::Little)
}

const ZLIB_BLOCK_SIZE: usize = 0x10000;

struct Decompressor<'a, R>
where
    R: Read,
{
    src: &'a mut R,
    compressed_block: Box<[u8; ZLIB_BLOCK_SIZE]>,
    buf: Box<[u8; ZLIB_BLOCK_SIZE]>,
    buf_pos: usize,
    buf_end: usize,
    eof: bool,
    blocks_decompressed: usize,
    uncompressed_size: usize,
}

impl<'a, R> Decompressor<'a, R>
where
    R: Read,
{
    fn new(src: &'a mut R, uncompressed_size: usize) -> Self {
        Decompressor {
            src,
            compressed_block: Box::new([0; ZLIB_BLOCK_SIZE]),
            buf: Box::new([0; ZLIB_BLOCK_SIZE]),
            buf_pos: 0,
            buf_end: 0,
            eof: uncompressed_size == 0,
            blocks_decompressed: 0,
            uncompressed_size,
        }
    }
}

impl<'a, R> Read for Decompressor<'a, R>
where
    R: Read,
{
    fn read(&mut self, buf: &mut [u8]) -> std::io::Result<usize> {
        use std::io::{Error, ErrorKind};

        let mut bytes_left = self.buf_end - self.buf_pos;
        if bytes_left == 0 {
            if self.eof {
                return Ok(0);
            }
            let compressed_block_size: usize = read32(self.src)?.try_into().unwrap();
            if compressed_block_size > ZLIB_BLOCK_SIZE {
                return Err(Error::new(
                    ErrorKind::Other,
                    format!(
                        "compressed block size {} is too large",
                        compressed_block_size
                    ),
                ));
            }
            self.src
                .read_exact(&mut self.compressed_block[..compressed_block_size])?;

            // TODO possible to bypass the internal buffers if the destination is large enough
            if compressed_block_size == ZLIB_BLOCK_SIZE {
                // no compression
                self.buf.copy_from_slice(&self.compressed_block[..]);
            } else {
                let mut zlib = ZlibDecoder::new(&self.compressed_block[..compressed_block_size]);
                zlib.read_exact(&mut self.buf[..])?;
                if zlib.bytes().next().is_some() {
                    return Err(Error::new(ErrorKind::Other, "zlib didn't use all bytes"));
                }
            }

            self.buf_pos = 0;
            self.blocks_decompressed += 1;
            if self.blocks_decompressed * ZLIB_BLOCK_SIZE >= self.uncompressed_size {
                self.eof = true;
                self.buf_end =
                    self.uncompressed_size - (self.blocks_decompressed - 1) * ZLIB_BLOCK_SIZE;
            } else {
                self.buf_end = ZLIB_BLOCK_SIZE;
            }
            bytes_left = self.buf_end;
        }

        let to_copy = std::cmp::min(buf.len(), bytes_left);
        let copy_end = self.buf_pos + to_copy;
        buf[..to_copy].copy_from_slice(&self.buf[self.buf_pos..copy_end]);
        self.buf_pos = copy_end;

        Ok(to_copy)
    }
}

// much of this list is from hashdict.txt in id-daemon's bitsquid tools
// https://zenhax.com/viewtopic.php?t=1019
const NAMES: &[&str] = &[
    "timpani_master",
    "strings",
    "bones",
    "render_config",
    "level",
    "input",
    "network_config",
    "ugg",
    "xml",
    "wav",
    "baked_lighting",
    "config",
    "data",
    "flow",
    "animation",
    "timpani_bank",
    "shader_library_group",
    "font",
    "lua",
    "state_machine",
    "particles",
    "upb",
    "package",
    "surface_properties",
    "mouse_cursor",
    "physics_properties",
    "shader",
    "texture",
    "sound_environment",
    "animation_curves",
    "unit",
    "static_pvs",
    "shader_library",
    "material",
    "vector_field",
    "spu_job",
    "ivf",
    "shading_environment",
    "wwise_stream",
    "wwise_bank",
    "wwise_metadata",
    "wwise_dep",

    // Tamarin names from the wwise_dep files
    "wwise/explore_02",
    "wwise/explore_03",
    "wwise/mantis",
    "wwise/explore_01",
    "wwise/action_02",
    "wwise/tamarin_home_day",
    "wwise/menu",
    "wwise/Init",
    "wwise/intro",
    "wwise/gui",
    "wwise/factory",
    "wwise/caves_01",
    "wwise/objects",
    "wwise/physics",
    "wwise/characters",
    "wwise/weapons",
    "wwise/environments",
    "wwise/tamarin_rescue",
    "wwise/toxic_woods",
    "wwise/tamarin_home",
    "wwise/action_03",
    "wwise/action_01",
    "wwise/caves_02",
    "wwise/action_03_ambient",
];

// missing extensions:
// c17b8adddbfbc068 (in 120747090c8edcba and 5d92590964a10073)
// 7910103158fc1de9 (in 209fb8c3c0a8c3a4)
// a59326f837e99332 (in 834f4fa8c15da34b)
// 394e4fef3b347ba9 (in d76577146440f4cf)

fn hash_name(n: &str) -> u64 {
    murmur_hash64a(n.as_bytes(), 0)
}

fn should_dump(_ext_hash: u64) -> bool {
    true
}

lazy_static! {
    static ref NAME_HASHES: HashMap<u64, String> = {
        let mut map = HashMap::new();
        for &n in NAMES {
            map.insert(hash_name(n), String::from(n));
        }
        map
    };
}

fn file_name(ext_hash: u64, name_hash: u64) -> String {
    format!(
        "{}.{}",
        if let Some(n) = NAME_HASHES.get(&name_hash) {
            n.clone()
        } else {
            //unknown_hashes.insert(ext_hash);
            format!("{:08X}", name_hash)
        },
        if let Some(n) = NAME_HASHES.get(&ext_hash) {
            n.clone()
        } else {
            //unknown_hashes.insert(ext_hash);
            //panic!("unknown extension {:08x}", ext_hash);
            format!("{:08X}", ext_hash)
        }
    )
}

fn fallible_main_dump() -> Result<(), Box<dyn Error>> {
    fn read32(f: &mut impl Read) -> u32 {
        f.ioread_with(Endian::Little).expect("u32")
    }

    fn read64(f: &mut impl Read) -> u64 {
        f.ioread_with(Endian::Little).expect("u64")
    }

    let infile_path: PathBuf = std::env::args_os().nth(1).expect("infile arg").into();

    let mut compressed = BufReader::new(
        OpenOptions::new()
            .read(true)
            .open(&infile_path)
            .expect("infile open"),
    );

    let mut f = {
        let magic = read32(&mut compressed);
        let unpacked_size = read32(&mut compressed);
        let unk0 = read32(&mut compressed);
        assert_eq!(magic, 0xf0000007); // v7?
        assert_eq!(unk0, 0);

        Decompressor::new(&mut compressed, unpacked_size.try_into()?)
    };

    // create output dir
    let outdir_path = infile_path.with_file_name(format!(
        "{}_dir",
        infile_path
            .file_name()
            .unwrap()
            .to_str()
            .ok_or("bad unicode in file name")?
    ));
    if outdir_path.is_dir() {
        return Err(format!("output directory {} already exists", outdir_path.display()).into());
    }
    std::fs::create_dir(&outdir_path)?;

    let quiet = true;

    let magic = read32(&mut f);
    let subfile_count = read32(&mut f);
    let other_count = read32(&mut f);
    assert_eq!(magic, 1);

    if !quiet {
        println!("{} files", subfile_count);
    }

    // read through index
    if !quiet {
        println!("index");
    }
    for i in 0..subfile_count {
        let extension_hash = read64(&mut f);
        let name_hash = read64(&mut f);
        let name = file_name(extension_hash, name_hash);
        if !quiet {
            println!(" {:4}: {}", i, name);
        }
    }

    // read directory index?
    if !quiet && other_count > 0 {
        println!();
        println!("other index?");
    }
    for i in 0..other_count {
        let unk1 = read64(&mut f);
        let unk2 = read64(&mut f);
        let unk3 = read64(&mut f);
        let unk4 = read64(&mut f);

        if !quiet {
            println!(" {:4} {:08X} {:08X} {:08X} {:X}", i, unk1, unk2, unk3, unk4);
        }
    }

    // read each file
    if !quiet {
        println!();
        println!("files");
    }
    for i in 0..subfile_count {
        let extension_hash = read64(&mut f);
        let name_hash = read64(&mut f);
        let name = file_name(extension_hash, name_hash);
        if !quiet {
            println!(" {:4}: {}", i, name);
        }

        let magic = read32(&mut f);
        let unk1 = read32(&mut f);
        let body_size = read32(&mut f);
        let unk2 = read32(&mut f);

        assert_eq!(magic, 1);
        assert_eq!(unk1, 0);
        assert_eq!(unk2, 0);

        //println!("  {} bytes body", body_size);
        //f.seek(SeekFrom::Current(body_size.into()))?;
        let mut limit = f.take(body_size.into());
        if should_dump(extension_hash) {
            let mut outfile_path = outdir_path.to_path_buf();

            let components: Vec<_> = name.split('/').collect();
            if components.len() > 1 {
                for c in &components[..components.len() - 1] {
                    outfile_path.push(c);
                }
                std::fs::create_dir_all(&outfile_path)?;
            }
            outfile_path.push(&components.last().unwrap());

            let mut multiplicity = 0;
            while outfile_path.is_file() {
                multiplicity += 1;
                if multiplicity > subfile_count {
                    return Err(format!("couldn't find a unique name for {}", name).into());
                }
                let (stem, ext) = name.split_at(name.find('.').unwrap());
                outfile_path.set_file_name(format!("{}_{}{}", stem, multiplicity, ext));
            }
            let mut outfile = OpenOptions::new()
                .write(true)
                .create_new(true)
                .open(&outfile_path)
                .map_err(|e| format!("opening {}, error:\n{}", outfile_path.display(), e))?;

            std::io::copy(&mut limit, &mut outfile)?;
            outfile.flush()?;
        } else {
            // TODO seek
            std::io::copy(&mut limit, &mut std::io::sink())?;
        }
        f = limit.into_inner();
    }

    assert!(f.bytes().next().is_none());
    assert!(compressed.bytes().next().is_none());

    Ok(())
}
