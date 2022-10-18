use std::collections::{hash_map::Entry, HashMap};
use std::fs::OpenOptions;
use std::io::{BufReader, Read, Seek, SeekFrom, Write};
use std::path::PathBuf;
use std::convert::TryInto;

use crc::{Crc, CRC_32_ISO_HDLC};
use nom::{
    branch::alt,
    bytes::complete::{tag, take_till, take_until},
    character::complete::{char, digit1, not_line_ending, multispace0},
    combinator::{map_res, opt},
    multi::{many0, many_m_n},
    sequence::{delimited, preceded, separated_pair, terminated, tuple},
    IResult,
    Parser,
};
use scroll::{Endian, IOread};

fn main() {
    if let Err(e) = fallible_main() {
        eprintln!("{}", e);
    }
}

fn read32(f: &mut impl Read) -> std::io::Result<u32> {
    f.ioread_with(Endian::Big)
}

const CRC32: Crc<u32> = Crc::<u32>::new(&CRC_32_ISO_HDLC);
fn crc32(s: &str) -> u32 {
    CRC32.checksum(s.as_bytes())
}

const AAA_MAXSOUNDS: u32 = 7191; // sound.h
fn fallible_main() -> Result<(), Box<dyn std::error::Error>> {
    let path_hashes = (0..AAA_MAXSOUNDS)
        .flat_map(|i| {
            [
                format!("data\\sound\\{:07}.mdf", i),
                format!("data\\sound\\{:07}.dsp", i),
            ]
        })
        .chain(std::iter::once("data\\sound\\vexxaudio.ad".into()))
        .map(|s| (crc32(&s), s))
        .collect::<HashMap<u32, String>>();

    assert!(std::env::args_os().len() == 2);
    let tre_path: PathBuf = std::env::args_os().nth(1).expect("infile arg").into();
    let mut tre_file = BufReader::new(OpenOptions::new().read(true).open(&tre_path)?);

    let file_count = read32(&mut tre_file)?;
    assert!(file_count < 100000);
    let mut file_index = HashMap::<u32, FileEntry>::new();

    for i in 0..file_count {
        let offset = read32(&mut tre_file)?;
        let size = read32(&mut tre_file)?;
        let name_crc = read32(&mut tre_file)?;
        let body_crc = read32(&mut tre_file)?;

        if let Some(name) = path_hashes.get(&name_crc) {
            //println!("{:4}: {}", i, name);
        } else {
            println!(
                "{:4}: {:x} {:x} {:08x} {:08x} unknown",
                i, offset, size, name_crc, body_crc
            );
        }

        file_index.insert(name_crc, FileEntry { offset, size, body_crc });
    }

    // sound.ad9 - item to file name mapping
    /*
    let report_path = tre_path.with_file_name("sound.ad9");
    let mut report = String::new();
    OpenOptions::new()
        .read(true)
        .open(&report_path)?
        .read_to_string(&mut report)?;

    let report = parse_entry(&report).unwrap().1;
    assert_eq!(report.ty, "SOUND_INFO_FILE");
    //println!("{}", report.val);

    let mut sound_items = HashMap::<u32, Vec<SoundFile>>::new();
    for item in report.sub_entry.unwrap() {
        assert_eq!(item.ty, "PUBLISHED_SOUND_ITEM");
        //println!("  {}", item.val);

        let item_idx = parse_quoted_u32(item.val).unwrap().1;
        let mut files: Vec<_> = item.sub_entry.unwrap().iter().map(|file| {
            assert_eq!(file.ty, "SOUND_FILE");
            assert!(file.sub_entry.is_none());
            //println!("    {}", file.val);

            let sound_file_entry = parse_sound_file_entry(file.val).unwrap().1;

            let crc = crc32(&format!("data\\sound\\{}", sound_file_entry.name));
            assert!(path_hashes.contains_key(&crc));

            SoundFile { size: sound_file_entry.size, crc }
        }).collect();

        match sound_items.entry(item_idx) {
            Entry::Occupied(_) => panic!(),
            Entry::Vacant(v) => v.insert(files),
        };
    }
    */

    // sound.ad3
    let audio_data_path = tre_path.with_file_name("sound.ad3");
    let mut audio_data = String::new();
    OpenOptions::new()
        .read(true)
        .open(&audio_data_path)?
        .read_to_string(&mut audio_data);

    let audio_data = parse_audio_data(&audio_data).unwrap().1;

    let audio_data = match audio_data {
        AudioDataEntry::Nested(ad) => ad,
        _ => panic!(),
    };
    assert_eq!(audio_data[0], AudioDataEntry::Bullet("AUDIO_DATA"));
    let audio_data = match &audio_data[1] {
        AudioDataEntry::Nested(ad) => ad,
        _ => panic!(),
    };
    let music_group_idx = audio_data.iter().position(|e| match e { AudioDataEntry::Bullet(n) => n == &"Group = \"Music\"", _ => false }).unwrap();
    let music_group = &audio_data[music_group_idx + 1];
    dump_music(&mut tre_file, &file_index, &path_hashes, music_group)?;

    Ok(())
}

struct FileEntry {
    offset: u32,
    size: u32,
    body_crc: u32,
}

fn dump_music<T>(mut tre_file: &mut T, file_index: &HashMap<u32, FileEntry>, path_hashes: &HashMap<u32, String>, ad: &AudioDataEntry) -> Result<(), Box<dyn std::error::Error>> where T: Read + Seek {
    let entries = match ad {
        AudioDataEntry::Nested(entries) => entries,
        _ => panic!(),
    };
    let mut i = 0;
    while i < entries.len() {
        let sound_line = if let AudioDataEntry::Bullet(sound_line) = entries[i] {
            sound_line
        } else { panic!(); };
        let (name, value) = parse_line(sound_line).unwrap().1;
        let value = parse_quoted_string(value).unwrap().1;

        i += 1;
        if name == "Sound" {
            let sids = if let AudioDataEntry::Nested(sids) = &entries[i] {
                sids
            } else { panic!(); };

            for (j, sid) in sids.iter().enumerate() {
                let sid_line = if let AudioDataEntry::Bullet(s) = sid {s} else {panic!();};
                let (sid_lit, sid) = parse_line(sid_line).unwrap().1;
                if sid_lit == "SID" {
                    assert_eq!(j, 0);
                    let sid: u32 = sid.parse().unwrap();
                    /*
                    if let Some(item) = sound_items.get(&sid) {
                        for (k, file) in item.iter().enumerate() {
                            let filename = path_hashes.get(&file.crc).unwrap();
                            println!("{}[{}][{}] {}", value, j, k, filename);
                        }
                    } else {
                        println!("unknown sid {}", sid);
                    }
                    */
                    // TODO also .mdf
                    let path = format!("data\\sound\\{:07}.dsp", sid);
                    let crc = crc32(&path);
                    assert!(path_hashes.contains_key(&crc));
                    //println!("{} {}", value, path);

                    let file_entry_opt = file_index.get(&crc);
                    if file_entry_opt.is_none() {
                        println!("skipping missing {} {}", value, path);
                        continue;
                    }

                    let outfile_name = &format!("{}_{:07}.dsp", value, sid);
                    let mut outfile = OpenOptions::new()
                        .write(true)
                        .create_new(true)
                        .open(&outfile_name)
                        .map_err(|e| format!("opening {}, error:\n{}", outfile_name, e))?;

                    let file_entry = file_entry_opt.unwrap();
                    tre_file.seek(SeekFrom::Start(file_entry.offset as u64));
                    let mut limited = tre_file.take(file_entry.size as u64);
                    std::io::copy(&mut limited, &mut outfile);
                    outfile.flush()?;

                    tre_file = limited.into_inner();
                }
            }
        } else if name == "Group" {
            println!("{}", sound_line);
            dump_music(tre_file, file_index, path_hashes, &entries[i])?;
        } else { panic!(); }
        i += 1;
    }

    Ok(())
}

fn parse_line(b: &str) -> IResult<&str, (&str, &str)> {
    separated_pair(take_until(" = "), tag(" = "), nom::combinator::rest)(b)
}

#[derive(Debug)]
struct ReportEntry<'a> {
    ty: &'a str,
    val: &'a str,
    sub_entry: Option<Vec<Self>>,
}
fn parse_entry<'a>(b: &'a str) -> IResult<&'a str, ReportEntry<'a>> {
    let (rest, (ty, maybe_size)) = preceded(
        tuple((multispace0, char('*'))),
        tuple((
            take_till(|c| c == '[' || c == ' '),
            opt(
            delimited(char('['), map_res(take_until("]"), |s: &str| s.parse()), char(']'))),
        )),
    )(b)?;

    let (rest, value_to_eol) = preceded(tuple((multispace0, char('='), multispace0)),
        terminated(take_until("\n"), char('\n')))(rest)?;

    let (rest, sub_entry) = if let Some(size) = maybe_size {
        delimited(
            tuple((multispace0, char('{'))),
            many_m_n(1, size, parse_entry).map(Some),
            tuple((multispace0, char('}')))
                 )(rest)?
    } else {
        (rest, None)
    };

    return Ok((
        rest,
        ReportEntry {
            ty,
            val: value_to_eol,
            sub_entry,
        },
    ));
}

#[derive(Debug)]
struct SoundFile {
    size: u32,
    crc: u32,
}

#[derive(Debug)]
struct SoundFileEntry<'a> {
    name: &'a str,
    size: u32,
}

fn parse_sound_file_entry<'a>(b: &'a str) -> IResult<&'a str, SoundFileEntry<'a>> {
    separated_pair(delimited(char('"'), take_until("\""), char('"')), tag(", "), map_res(digit1, |s: &str| s.parse())).map(|(name, size)| SoundFileEntry { name, size }).parse(b)
}

fn parse_quoted_string<'a>(b: &'a str) -> IResult<&'a str, &'a str> {
    delimited(char('"'), take_until("\""), char('"')).parse(b)
}

fn parse_quoted_u32<'a>(b: &'a str) -> IResult<&'a str, u32> {
    delimited(char('"'), map_res(digit1, |s: &str| -> Result<_, std::num::ParseIntError> {s.parse()}), char('"')).parse(b)
}

#[derive(Debug, PartialEq, Eq)]
enum AudioDataEntry<'a> {
    Bullet(&'a str),
    Comment(&'a str),
    Stringy(&'a str),
    Nested(Vec<Self>),
}

fn parse_audio_data<'a>(b: &'a str) -> IResult<&'a str, AudioDataEntry<'a>> {
    many0(
    preceded(multispace0,
        alt((
            preceded(char('*'), not_line_ending).map(AudioDataEntry::Bullet),
            preceded(char(';'), not_line_ending).map(AudioDataEntry::Comment),
            delimited(char('"'), take_until("\""), tuple((char('"'), opt(char(','))))).map(AudioDataEntry::Stringy),
            delimited(char('{'), parse_audio_data, tuple((multispace0, char('}')))),
            )
            ))).map(AudioDataEntry::Nested).parse(b)
}
