/*
  Copyright 2011  Jeff Abrahamson
  
  This file is part of srd.
  
  srd is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  
  srd is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with srd.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <bzlib.h>
#include <iostream>
#include <stdexcept>
#include <string>

#include "compress.h"
#include "mode.h"


using namespace srd;
using namespace std;


/*
  The bzip2 package is documented at

  http://www.bzip.org/1.0.3/html/index.html
*/


/*
  Compress a string.
*/
string srd::compress(const string &in_buf)
{
        // As documented at http://www.bzip.org/1.0.3/html/util-fns.html
        unsigned int output_max_size = static_cast<double>(in_buf.size()) * 1.06 + 600.5;
        char out_buf_raw[output_max_size];

        // For argument definitions, cf.
        //   http://www.bzip.org/1.0.3/html/util-fns.html#bzbufftobuffdecompress
        // and also
        //   http://www.bzip.org/1.0.3/html/low-level.html#bzcompress-init
        //
        // Source string is not changed, one understands, but the C origin means
        // a char * rather than a const char *.  So const_cast.
        int ret = BZ2_bzBuffToBuffCompress(out_buf_raw,
                                           &output_max_size,
                                           const_cast<char *>(in_buf.c_str()),
                                           in_buf.size(),
                                           1, // blockSize100k, in the range 1..9
                                           (const bool)mode(Verbose),
                                           0  // workFactor
                                           );
        string the_error;
        switch(ret) {
        case BZ_CONFIG_ERROR:
                the_error = "The bzip2 library has been mis-compiled.";
                cerr << the_error << endl;
                throw(runtime_error(the_error));
        case BZ_PARAM_ERROR:
                the_error = "Parameter error in BZ2_bzBuffToBuffCompress";
                cerr << the_error << endl;
                cerr << "Cf. http://www.bzip.org/1.0.3/html/util-fns.html#bzbufftobuffcompress"
                     << endl;
                throw(invalid_argument(the_error));
        case BZ_MEM_ERROR:
                the_error = "Insufficient memory available for compression";
                cerr << the_error << endl;
                throw(length_error(the_error));
        case BZ_OUTBUFF_FULL:
                the_error = "The size of the compressed data exceeds *destLen";
                cerr << the_error << endl;
                throw(length_error(the_error));
        case BZ_OK:
                break;
        default:
                the_error = "Unexpected return from BZ2_bzBuffToBuffCompress";
                cerr << the_error << endl;
                throw(logic_error(the_error));
        };
        
        string out_buf(out_buf_raw, output_max_size);
        return(out_buf);
}



/*
  Decompress a string.
*/
string srd::decompress(const string &in_buf,
                  unsigned int uncompressed_size_hint)
{
        if(0 == uncompressed_size_hint)
                uncompressed_size_hint = 5 * in_buf.size();
        char out_buf_raw[uncompressed_size_hint];

        // For argument definitions, cf.
        //   http://www.bzip.org/1.0.3/html/util-fns.html#bzbufftobuffdecompress
        // and also
        //   http://www.bzip.org/1.0.3/html/low-level.html#bzDecompress-init
        //
        // Source string is not changed, one understands, but the C origin means
        // a char * rather than a const char *.  So const_cast.
        int ret = BZ2_bzBuffToBuffDecompress(out_buf_raw,
                                             &uncompressed_size_hint,
                                             const_cast<char *>(in_buf.c_str()),
                                             in_buf.size(),
                                             false, // small is false: else slower
                                             mode(Verbose)
                                             );

        string the_error;
        switch(ret) {
        case BZ_CONFIG_ERROR:
                the_error = "The bzip2 library has been mis-compiled.";
                cerr << the_error << endl;
                throw(runtime_error(the_error));
        case BZ_PARAM_ERROR:
                the_error = "Parameter error in BZ2_bzBuffToBuffDecompress";
                cerr << the_error << endl;
                cerr << "Cf. http://www.bzip.org/1.0.3/html/util-fns.html#bzbufftobuffdecompress"
                     << endl;
                throw(invalid_argument(the_error));
        case BZ_MEM_ERROR:
                the_error = "Insufficient memory available for decompression";
                cerr << the_error << endl;
                throw(length_error(the_error));
        case BZ_OUTBUFF_FULL:
                cout << "The size of the compressed data exceeds *destLen, trying *= 2."
                     << endl;
                return decompress(in_buf, 2 * uncompressed_size_hint);
        case BZ_DATA_ERROR:
                the_error = "Data integrity error was detected in the compressed data.";
                cerr << the_error << endl;
                throw(domain_error(the_error));
        case BZ_DATA_ERROR_MAGIC:
                the_error = "Compressed data doesn't begin with the right magic bytes.";
                cerr << the_error << endl;
                throw(domain_error(the_error));
        case BZ_UNEXPECTED_EOF:
                the_error = "Compressed data ends unexpectedly.";
                cerr << the_error << endl;
                throw(domain_error(the_error));
        case BZ_OK:
                break;
        default:
                the_error = "Unexpected return from BZ2_bzBuffToBuffDecompress";
                cerr << the_error << endl;
                throw(logic_error(the_error));
        };
        
        string out_buf(out_buf_raw, uncompressed_size_hint);
        return(out_buf);
}


