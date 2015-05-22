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

#pragma once

#include <getopt.h>
#include <stdlib.h>
#include <iostream>
#include <vector>

#include <common_args.hpp>
#include <str_utils.hpp>

using std::cout;
using std::cerr;
using std::endl;
using std::vector;
using std::string;
using std::ostringstream;

namespace kat
{
    const string    DEFAULT_OUTPUT_PREFIX   ="kat-sect";
    const uint16_t  DEFAULT_THREADS         = 1;
    const uint16_t  DEFAULT_GC_BINS         = 1001;
    const uint16_t  DEFAULT_CVG_BINS        = 1001;
    const bool      DEFAULT_CVG_LOG         = false;
    const bool      DEFAULT_BOTH_STRANDS    = false;
    const bool      DEFAULT_NO_COUNT_STATS  = false;
    const bool      DEFAULT_MEDIAN          = false;

    const uint16_t MIN_ARGS = 1;

    class SectArgs : public BaseArgs {
    protected:

        // ***********************************************
        // These methods override BaseArgs virtual methods

        const string usage() const {
            return "Usage: kat sect [options] -s <sequence_file> <jellyfish_hash>";
        }

        const string shortDescription() const {
            return "Estimates coverage levels for a collection of sequences using jellyfish K-mer counts.";
        }

        const string longDescription() const {
            string long_desc = "This tool will produce a fasta style file containing K-mer coverage counts mapped across each " \
                                "sequence.  In addition, a space separated table file containing the mean coverage score and GC " \
                                "of each sequence is produced.  The row order is identical to the original sequence file. </br> " \
                                "Note: K-mers containing any Ns derived from sequences in the sequence file not be included.";

            return lineBreakString(long_desc, 78, "  ");
        }

        const string optionsDescription() const {
            ostringstream help_str;

            help_str << " -s, --sequences=path        *Sequence file in either fasta or fastq format (gzipped files supported)." << endl
                     << "                             Contains sequences that should have their coverage estimated." << endl
                     << " -o, --output_prefix=string  Path prefix for files generated by this program (\"" << DEFAULT_OUTPUT_PREFIX << "\")." << endl
                     << " -x, --gc_bins=uint16        Number of bins for the gc data when creating the contamination matrix (" << DEFAULT_GC_BINS << ")." << endl
                     << " -y, --cvg_bins=uint16       Number of bins for the cvg data when creating the contamination matrix (" << DEFAULT_CVG_BINS << ")." << endl
                     << " -l, --cvg_logscale          Compresses cvg scores into logscale for determining the cvg bins within the" << endl
                     << "                             contamination matrix. Otherwise compresses cvg scores by a factor of 0.1 into" << endl
                     << "                             the available bins (" << DEFAULT_CVG_LOG << ")." << endl
                     << " -t, --threads=uint16        The number of threads to use (" << DEFAULT_THREADS << ")." << endl
                     << " -C, --both_strands          IMPORTANT: Whether the jellyfish hashes contains K-mers produced for both" << endl
                     << "                             strands.  If this is not set to the same value as was produced during jellyfish" << endl
                     << "                             counting then output from sect will be unpredicatable (" << DEFAULT_BOTH_STRANDS << ")." << endl
                     << " -n, --no_count_stats        Tells SECT not to output count stats.  Sometimes when using SECT on read files" << endl
                     << "                             the output can get very large.  When flagged this just outputs summary stats for" << endl
                     << "                             each sequence." << endl
                     << " -m, --median                When calculating average sequence coverage, use median rather than the mean kmer" << endl
                     << "                             frequency.";

            return help_str.str();
        }

        vector<option>* longOptions()
        {
            static struct option long_options_array[] =
            {
                {"sequences",       required_argument,  0, 's'},
                {"output_prefix",   required_argument,  0, 'o'},
                {"gc_bins",         required_argument,  0, 'x'},
                {"cvg_bins",        required_argument,  0, 'y'},
                {"cvg_logscale",    no_argument,        0, 'l'},
                {"threads",         required_argument,  0, 't'},
                {"both_strands",    no_argument,        0, 'C'},
                {"no_count_stats",  no_argument,        0, 'n'},
                {"median",          no_argument,        0, 'm'}
            };

            vector<option>* long_options = new vector<option>();

            for(uint8_t i = 0; i < 9; i++)
            {
                long_options->push_back(long_options_array[i]);
            }

            return long_options;
        }

        string shortOptions()
        {
            return "s:o:x:y:lt:Cnm";
        }

        void setOption(int c, string& option_arg) {

            switch(c)
            {
            case 's':
                seq_file = string(option_arg);
                break;
            case 'o':
                output_prefix = string(option_arg);
                break;
            case 't':
                threads_arg = strToInt16(option_arg);
                break;
            case 'x':
                gc_bins = strToInt16(option_arg);
                break;
            case 'y':
                cvg_bins = strToInt16(option_arg);
                break;
            case 'C':
                both_strands = true;
                break;
            case 'l':
                cvg_logscale = true;
                break;
            case 'n':
                no_count_stats = true;
                break;
            case 'm':
                median = true;
                break;
            }
        }

        void processRemainingArgs(const vector<string>& remaining_args) {
            jellyfish_hash = remaining_args[0];
        }

        const string currentStatus() const {
            ostringstream status;

            status << "Sequence file: " << seq_file << endl
                   << "Number of GC bins for matrix: " << gc_bins << endl
                   << "Number of coverage bins for matrix: " << cvg_bins << endl
                   << "Compress coverage scores to logscale: " << cvg_logscale << endl
                   << "Threads requested: " << threads_arg << endl
                   << "Jellyfish hash: " << jellyfish_hash << endl
                   << "Output prefix: " << output_prefix << endl
                   << "Jellyfish hash double stranded: " << both_strands << endl
                   << "Do NOT output count stats: " << no_count_stats << endl
                   << "Use median (rather than mean) for average kmer coverage: " << median << endl;

            return status.str().c_str();
        }

    public:
        string      seq_file;
        string      jellyfish_hash;
        string      output_prefix;
        uint16_t    gc_bins;
        uint16_t    cvg_bins;
        bool        cvg_logscale;
        uint16_t    threads_arg;
        bool        both_strands;
        bool        no_count_stats;
        bool        median;

        // Default constructor

        SectArgs() : BaseArgs(MIN_ARGS),
            seq_file(""), output_prefix(DEFAULT_OUTPUT_PREFIX), gc_bins(DEFAULT_GC_BINS), cvg_bins(DEFAULT_CVG_BINS),
            cvg_logscale(DEFAULT_CVG_LOG), threads_arg(DEFAULT_THREADS), both_strands(DEFAULT_BOTH_STRANDS),
            no_count_stats(DEFAULT_NO_COUNT_STATS), median(DEFAULT_MEDIAN)
        {}

        // Constructor that parses command line options

        SectArgs(int argc, char* argv[]) : BaseArgs(MIN_ARGS),
            seq_file(""), output_prefix(DEFAULT_OUTPUT_PREFIX), gc_bins(DEFAULT_GC_BINS), cvg_bins(DEFAULT_CVG_BINS),
            cvg_logscale(DEFAULT_CVG_LOG), threads_arg(DEFAULT_THREADS), both_strands(DEFAULT_BOTH_STRANDS),
            no_count_stats(DEFAULT_NO_COUNT_STATS), median(DEFAULT_MEDIAN)
        {
            parse(argc, argv);
        }

        ~SectArgs() {
        }
    };
}
