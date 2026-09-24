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
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "srd.h"

using namespace srd;
using namespace std;

/*
  The bzip2 package is documented at

  http://www.bzip.org/1.0.3/html/index.html
*/

/*
  Compress a string.
*/
string srd::compress(const string &in_buf) {
  // As documented at http://www.bzip.org/1.0.3/html/util-fns.html
  double max_size = static_cast<double>(in_buf.size()) * 1.06 + 600.5;
  if (max_size > numeric_limits<unsigned int>::max())
    throw(length_error("Record too large to compress."));
  unsigned int output_max_size = max_size;
  // On the heap:  a stack buffer overflowed the stack for records of
  // a few megabytes.
  string out_buf(output_max_size, '\0');

  // For argument definitions, cf.
  //   http://www.bzip.org/1.0.3/html/util-fns.html#bzbufftobuffdecompress
  // and also
  //   http://www.bzip.org/1.0.3/html/low-level.html#bzcompress-init
  //
  // Source string is not changed, one understands, but the C origin means
  // a char * rather than a const char *.  So const_cast.
  int ret = BZ2_bzBuffToBuffCompress(out_buf.data(), &output_max_size,
                                     const_cast<char *>(in_buf.c_str()),
                                     in_buf.size(),
                                     1, // blockSize100k, in the range 1..9
                                     (const bool)mode(Verbose),
                                     0 // workFactor
                                     );
  string the_error;
  switch (ret) {
  case BZ_CONFIG_ERROR:
    the_error = "The bzip2 library has been mis-compiled.";
    cerr << the_error << endl;
    throw(runtime_error(the_error));
  case BZ_PARAM_ERROR:
    the_error = "Parameter error in BZ2_bzBuffToBuffCompress";
    cerr << the_error << endl;
    cerr << "Cf. "
            "http://www.bzip.org/1.0.3/html/util-fns.html#bzbufftobuffcompress"
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

  out_buf.resize(output_max_size);
  return out_buf;
}

/*
  Decompress a string.

  Use bzip2's streaming interface, so that we needn't guess the size
  of the output:  append each chunk as it comes.
*/
string srd::decompress(const string &in_buf) {
  if (in_buf.size() > numeric_limits<unsigned int>::max())
    throw(length_error("Record too large to decompress."));

  // For argument definitions, cf.
  //   http://www.bzip.org/1.0.3/html/low-level.html#bzDecompress-init
  bz_stream stream = {};
  int ret = BZ2_bzDecompressInit(&stream, mode(Verbose),
                                 0 // small is false: else slower
                                 );
  string the_error;
  switch (ret) {
  case BZ_CONFIG_ERROR:
    the_error = "The bzip2 library has been mis-compiled.";
    cerr << the_error << endl;
    throw(runtime_error(the_error));
  case BZ_PARAM_ERROR:
    the_error = "Parameter error in BZ2_bzDecompressInit";
    cerr << the_error << endl;
    throw(invalid_argument(the_error));
  case BZ_MEM_ERROR:
    the_error = "Insufficient memory available for decompression";
    cerr << the_error << endl;
    throw(length_error(the_error));
  case BZ_OK:
    break;
  default:
    the_error = "Unexpected return from BZ2_bzDecompressInit";
    cerr << the_error << endl;
    throw(logic_error(the_error));
  };

  // Source string is not changed, one understands, but the C origin means
  // a char * rather than a const char *.  So const_cast.
  stream.next_in = const_cast<char *>(in_buf.data());
  stream.avail_in = in_buf.size();
  string out_buf;
  vector<char> chunk(64 * 1024);
  do {
    stream.next_out = chunk.data();
    stream.avail_out = chunk.size();
    ret = BZ2_bzDecompress(&stream);
    if (BZ_OK != ret && BZ_STREAM_END != ret)
      break;
    out_buf.append(chunk.data(), chunk.size() - stream.avail_out);
    // Input exhausted with room to spare but no end of stream:  the
    // data is truncated.
    if (BZ_OK == ret && 0 == stream.avail_in && 0 != stream.avail_out)
      ret = BZ_UNEXPECTED_EOF;
  } while (BZ_OK == ret);
  BZ2_bzDecompressEnd(&stream);

  switch (ret) {
  case BZ_PARAM_ERROR:
    the_error = "Parameter error in BZ2_bzDecompress";
    cerr << the_error << endl;
    throw(invalid_argument(the_error));
  case BZ_MEM_ERROR:
    the_error = "Insufficient memory available for decompression";
    cerr << the_error << endl;
    throw(length_error(the_error));
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
  case BZ_STREAM_END:
    break;
  default:
    the_error = "Unexpected return from BZ2_bzDecompress";
    cerr << the_error << endl;
    throw(logic_error(the_error));
  };

  return out_buf;
}
