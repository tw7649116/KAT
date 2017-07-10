//  ********************************************************************
//  This file is part of KAT - the K-mer Analysis Toolkit.
//
//  KAT is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  KAT is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with KAT.  If not, see <http://www.gnu.org/licenses/>.
//  *******************************************************************

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <thread>
#include <vector>
#include <fstream>
using std::thread;
using std::vector;
using std::fstream;

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/timer/timer.hpp>
namespace bfs = boost::filesystem;
using bfs::path;
using boost::lexical_cast;
using boost::timer::auto_cpu_timer;

#include <jellyfish/binary_dumper.hpp>
#include <jellyfish/err.hpp>
#include <jellyfish/file_header.hpp>
#include <jellyfish/mapped_file.hpp>
#include <jellyfish/mer_dna.hpp>
#include <jellyfish/jellyfish.hpp>
#include <jellyfish/large_hash_array.hpp>
#include <jellyfish/large_hash_iterator.hpp>
#include <jellyfish/storage.hpp>
using jellyfish::large_hash::reprobe_limit_t;
using jellyfish::Offsets;
using jellyfish::quadratic_reprobes;

#include <kat/jellyfish_helper.hpp>
#include <boost/algorithm/string/predicate.hpp>
using kat::JellyfishHelper;

/**
 * Extracts the jellyfish hash file header
 * @param jfHashPath Path to the jellyfish hash file
 * @return The hash header
 */
shared_ptr<file_header> kat::JellyfishHelper::loadHashHeader(const path& jfHashPath) {
    ifstream in(jfHashPath.c_str(), std::ios::in | std::ios::binary);
    shared_ptr<file_header> header = make_shared<file_header>(in);
    in.close();
    return header;
}

/**
 * Output header in human-readable format
 * @param header Jellyfish hash header
 * @param out Output stream
 */
void kat::JellyfishHelper::printHeader(const file_header& header, ostream& out) {

    out << "Jellyfish Header Info:" << endl;
    out << " - Cmdline: " << endl;
    for (string s : header.cmdline()) {
        out << s << " ";
    }
    out << endl;
    out << " - Format: " << header.format() << endl;
    out << " - Key length (bits): " << header.key_len() << endl;
    out << " - Value length (bits): " << header.val_len() << endl;
    out << " - Counter length (bytes): " << header.counter_len() << endl;
    out << " - # Hashes: " << header.nb_hashes() << endl;
    out << " - Max reprobe: " << header.max_reprobe() << endl;
    out << " - Max reprobe offset: " << header.max_reprobe_offset() << endl;
    out << " - Offset: " << header.offset() << endl;
    out << " - Size: " << header.size() << endl;
}

/**
 * Loads an existing jellyfish hash into memory
 * @param jfHashPath
 * @param verbose
 * @return 
 */
LargeHashArrayPtr kat::HashLoader::loadHash(const path& jfHashPath, bool verbose) {

    ifstream in(jfHashPath.c_str(), std::ios::in | std::ios::binary);
    header = file_header(in);

    if (!in.good()) {
        BOOST_THROW_EXCEPTION(JellyfishException() << JellyfishErrorInfo(string(
                "Failed to parse header of file: ") + jfHashPath.string()));
    }

    if (verbose) {
        kat::JellyfishHelper::printHeader(header, cerr);
    }

    if (header.format() == "bloomcounter") {
        in.close();
        BOOST_THROW_EXCEPTION(JellyfishException() << JellyfishErrorInfo(string(
                "KAT does not currently support bloom counted kmer hashes.  Please create a binary hash with jellyfish or KAT and use that instead.")));
    } else if (header.format() == text_dumper::format) {
        in.close();
        BOOST_THROW_EXCEPTION(JellyfishException() << JellyfishErrorInfo(string(
                "Processing a text format hash will be painfully slow, so we don't support it.  Please create a binary hash with jellyfish or KAT and use that instead.")));
    }
    else if (header.format() == binary_dumper::format) {

        // Makes sure jellyfish knows what size kmers we are working with.  The actual kmer size,
        // for our purposes, will be half of what the number of bits used to store it is.
        merLen = header.key_len() / 2;

        mer_dna::k(merLen);

        // Create a binary reader for the input file, configured using the header properties
        binary_reader reader(in, &header);

        // Create a binary map for the input file
        mapped_file map(jfHashPath.c_str());
        map.sequential(); // Prep for reading sequentially
        map.load(); // Load

        const char* dataStart = map.base() + header.offset();
        size_t fileSizeBytes = map.length() - header.offset();

        size_t key_len = header.key_len() / 8 + (header.key_len() % 8 != 0);
        size_t record_len = header.counter_len() + key_len;
        size_t nbRecords = fileSizeBytes / record_len;

        size_t lsize = jellyfish::ceilLog2(nbRecords * 2);
        size_t size_ = (size_t) 1 << lsize;

        if (verbose) {
            cerr << endl
                    << "Hash properties:" << endl
                    << " - Entry start location: " << (uint64_t) dataStart << endl
                    << " - Data size (in file): " << fileSizeBytes << endl
                    << " - Kmer length: " << merLen << endl
                    << " - Key length (bytes): " << key_len << endl
                    << " - Record size: " << record_len << endl
                    << " - # records: " << nbRecords << endl << endl;

            LargeHashArray::usage_info ui(header.key_len(), header.val_len(), header.max_reprobe());
            size_t memMb = (ui.mem(header.size()) / 1000000) + 1;
            cerr << "Approximate amount of RAM required for handling this hash (MB): " << memMb << endl;
        }

        if (fileSizeBytes % record_len != 0) {
            in.close();
            BOOST_THROW_EXCEPTION(JellyfishException() << JellyfishErrorInfo(string(
                    "Size of database (") + lexical_cast<string>(fileSizeBytes) +
                    ") must be a multiple of the length of a record (" + lexical_cast<string>(record_len) + ")"));
        }

        hash = new LargeHashArray(
                size_, // Make hash bigger than the file data round up to next power of 2
                header.key_len(),
                header.val_len(),
                header.max_reprobe());

        while (reader.next()) {
            hash->add(reader.key(), reader.val());
        }

        in.close();

        return hash;
    } else {
        in.close();
        BOOST_THROW_EXCEPTION(JellyfishException() << JellyfishErrorInfo(string(
                "Unknown format '") + header.format() + "'"));
    }

}

uint64_t kat::JellyfishHelper::getCount(LargeHashArrayPtr hash, const mer_dna& kmer, bool canonical) {
    const mer_dna k = canonical ? kmer.get_canonical() : kmer;
    uint64_t val = 0;
    hash->get_val_for_key(k, &val);
    return val;
}

/**
 * Simple count routine
 * @param ary Hash array which contains the counted kmers
 * @param parser The parser that handles the input stream and chunking
 * @param canonical whether or not the kmers should be treated as canonical or not
 */
void kat::JellyfishHelper::countSlice(HashCounter& ary, SequenceParser& parser, bool canonical, bool tenx) {

    MerIterator mers(parser, canonical, tenx);

    for (; mers; ++mers) {
        ary.add(*mers, 1);
    }

    ary.done();
}

/**
 * Counts kmers in the given sequence file (Fasta or Fastq) returning
 * a hash array of those kmers
 * @param seqFile Sequence file to count
 * @return The hash array
 */
LargeHashArrayPtr kat::JellyfishHelper::countSeqFile(const vector<path>& seqFiles, HashCounter& hashCounter, bool canonical, bool tenx, uint16_t threads) {

    // Convert paths to a format jellyfish is happy with
    vector<const char*> paths;
    for (auto& p : seqFiles) {
        paths.push_back(p.c_str());
    }

    // Ensures jellyfish knows what kind of kmers we are working with
    unsigned int merLen = hashCounter.key_len() / 2;
    mer_dna::k(merLen);

    StreamManager streams(paths.begin(), paths.end(), (const int) std::min(paths.size(), (size_t) threads));

    SequenceParser parser(merLen, streams.nb_streams(), 3 * threads, 4096, streams);

    vector<thread> t(threads);

    for (int i = 0; i < threads; i++) {
        t[i] = thread(&kat::JellyfishHelper::countSlice, std::ref(hashCounter), std::ref(parser), canonical, tenx);
    }

    for (int i = 0; i < threads; i++) {
        t[i].join();
    }

    return hashCounter.ary();
}

void kat::JellyfishHelper::dumpHash(LargeHashArrayPtr ary, file_header& header, uint16_t threads, const path& outputFile) {

    //JellyfishHelper::printHeader(header, cout);

    // Create the dumper
    binary_dumper dumper(4, ary->key_len(), threads, outputFile.c_str(), &header);
    dumper.one_file(true);
    dumper.dump(ary);
}

bool kat::JellyfishHelper::isPipe(const path& filename) {
    return boost::starts_with(filename.string(), "/proc") || boost::starts_with(filename.string(), "/dev");
}


/**
 * Returns whether or not the specified file path looks like it belongs to
 * a sequence file (either FastA or FastQ).  Gzipped sequence files are
 * also supported.
 * @param filename Path to file
 * @return Whether or not the file is a seqeunce file
 */
bool kat::JellyfishHelper::isSequenceFile(const path& filename) {

    // If we have a pipe as input then assume we are working with a sequence file
    if (JellyfishHelper::isPipe(filename)) return true;
    
    string ext = filename.extension().string();

    // Actually we can't handle gz files directly, so turning this off for now
    /*if (boost::iequals(ext, ".gz")) {
        string name = filename.filename().string();
        string shortName = name.substr(0, name.length() - 3);
        ext = path(shortName).extension().string();
    }*/

    // Check extension first
    bool seqext = boost::iequals(ext, ".fastq") ||
            boost::iequals(ext, ".fq") ||
            boost::iequals(ext, ".fasta") ||
            boost::iequals(ext, ".fa") ||
            boost::iequals(ext, ".fna") ||
            boost::iequals(ext, ".fas") ||
            boost::iequals(ext, ".scafSeq"); // For SOAP de novo scaffolder output


    if (seqext) return true;

    // Now check first character of the file
    char ch;
    fstream fin(filename.string(), fstream::in);
    fin >> ch;
    fin.close();
    
    if (ch == '>' || ch == '@') return true;
    
    // If we've got this far then it's not obviously a fasta or fastq file.
    return false;
}

