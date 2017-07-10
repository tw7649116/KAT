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

#include <stdint.h>
#include <iostream>
#include <math.h>
#include <memory>
#include <thread>
#include <vector>
using std::shared_ptr;
using std::make_shared;
using std::thread;
using std::vector;

#include <boost/algorithm/string.hpp>
#include <boost/exception/all.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;
namespace bfs = boost::filesystem;
using bfs::path;
using boost::lexical_cast;

#include <jellyfish/mer_dna.hpp>

#include <kat/jellyfish_helper.hpp>
#include <kat/input_handler.hpp>
#include <kat/matrix_metadata_extractor.hpp>
#include <kat/sparse_matrix.hpp>
using kat::InputHandler;
using kat::ThreadedSparseMatrix;


using std::ostream;

typedef boost::error_info<struct GcpError,string> GcpErrorInfo;
struct GcpException: virtual boost::exception, virtual std::exception { };


namespace kat {

    const string     DEFAULT_GCP_PLOT_OUTPUT_TYPE     = "png";
    
    class Gcp {
    private:

        // Input args
        InputHandler    input;
        path            outputPrefix;
        uint16_t        threads;
        double          cvgScale;
        uint16_t        cvgBins;
        bool            verbose;
        
        // Stores results
        shared_ptr<ThreadedSparseMatrix> gcp_mx; // Stores cumulative base count for each sequence where GC and CVG are binned

    public:

        Gcp(vector<path>& _inputs);

        virtual ~Gcp() {            
        }

        bool isCanonical() const {
            return input.canonical;
        }

        void setCanonical(bool canonical) {
            this->input.canonical = canonical;
        }
		
	bool isTenx() const {
            return input.tenx;
        }
		
	void setTenx(bool tenx) {
            this->input.tenx = tenx;
        }

        uint16_t getCvgBins() const {
            return cvgBins;
        }

        void setCvgBins(uint16_t cvgBins) {
            this->cvgBins = cvgBins;
        }

        double getCvgScale() const {
            return cvgScale;
        }

        void setCvgScale(double cvgScale) {
            this->cvgScale = cvgScale;
        }

        uint64_t getHashSize() const {
            return input.hashSize;
        }

        void setHashSize(uint64_t hashSize) {
            this->input.hashSize = hashSize;
        }

        uint16_t getMerLen() const {
            return input.merLen;
        }

        void setMerLen(uint16_t merLen) {
            this->input.merLen = merLen;
        }

        path getOutputPrefix() const {
            return outputPrefix;
        }

        void setOutputPrefix(path outputPrefix) {
            this->outputPrefix = outputPrefix;
        }

        uint16_t getThreads() const {
            return threads;
        }

        void setThreads(uint16_t threads) {
            this->threads = threads;
        }
        
        bool isDumpHash() const {
            return input.dumpHash;
        }

        void setDumpHash(bool dumpHash) {
            this->input.dumpHash = dumpHash;
        }

        bool isVerbose() const {
            return verbose;
        }

        void setVerbose(bool verbose) {
            this->verbose = verbose;
        }

        
        void execute();
        

        // Print K-mer comparison matrix

        void printMainMatrix(ostream &out);
        
        void save();
        
        void plot(const string& output_type);
        
    protected:
        
        void analyse();
        
        void analyseSlice(int th_id);
        
        void merge();
        
        static const string helpMessage() {
             return string("Usage: kat gcp (<input>)+\n\n") +
                            "Compares GC content and K-mer coverage from the input.\n\n" +
                            "This tool takes in either a single jellyfish hash or one or more FastA or FastQ input files " \
                            "and then counts the GC nucleotides for each distinct K-mer " \
                            "in the hash.  For each GC count and K-mer coverage level, the number of distinct K-mers are counted and " \
                            "stored in a matrix.  This matrix can be used to analyse biological content within the hash.  For example, " \
                            "it can be used to distinguish legitimate content from contamination, or unexpected content.\n\n" \
                            "Options";
        }
        
    public:
          
        static int main(int argc, char *argv[]);
    };
}
