/*
Copyright (c) 2023 - 2024 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
#pragma once

#include <memory>
#include <string>
#include <vector>
#include "rocparser.h"
#include "../commons.h"

typedef enum ParserResult {
    PARSER_OK                                   = 0,
    PARSER_FAIL                                    ,

// common errors
    PARSER_UNEXPECTED                              ,

    PARSER_ACCESS_DENIED                           ,
    PARSER_INVALID_ARG                             ,
    PARSER_OUT_OF_RANGE                            ,

    PARSER_OUT_OF_MEMORY                           ,
    PARSER_INVALID_POINTER                         ,

    PARSER_NO_INTERFACE                            ,
    PARSER_NOT_IMPLEMENTED                         ,
    PARSER_NOT_SUPPORTED                           ,
    PARSER_NOT_FOUND                               ,

    PARSER_ALREADY_INITIALIZED                     ,
    PARSER_NOT_INITIALIZED                         ,

    PARSER_INVALID_FORMAT                          ,// invalid data format

    PARSER_WRONG_STATE                             ,
    PARSER_FILE_NOT_OPEN                           ,// cannot open file
    PARSER_STREAM_NOT_ALLOCATED                    ,

// device common codes
    PARSER_NO_DEVICE                               ,

    //result codes
    PARSER_EOF                                     ,
    PARSER_REPEAT                                  ,

    //error codes
    PARSER_INVALID_DATA_TYPE                       ,//invalid data type
    PARSER_INVALID_RESOLUTION                      ,//invalid resolution (width or height)
    PARSER_CODEC_NOT_SUPPORTED                     ,//codec not supported
} ParserResult;

typedef struct {
    uint32_t numerator;
    uint32_t denominator;
} Rational;

#define ZEROBYTES_SHORTSTARTCODE 2 //indicates the number of zero bytes in the short start-code prefix
#define RBSP_BUF_SIZE 1024  // enough to parse any parameter sets or slice headers

/**
 * @brief Base class for video parsing
 * 
 */
class RocVideoParser {
public:
    RocVideoParser();    // default constructor
    RocVideoParser(RocdecParserParams *pParams) : parser_params_(*pParams) {};
    virtual ~RocVideoParser() = default ;
    virtual void SetParserParams(RocdecParserParams *pParams) { parser_params_ = *pParams; };
    RocdecParserParams *GetParserParams() {return &parser_params_;};
    virtual rocDecStatus Initialize(RocdecParserParams *pParams);
    virtual rocDecStatus ParseVideoData(RocdecSourceDataPacket *pData) = 0;     // pure virtual: implemented by derived class
    virtual rocDecStatus UnInitialize() = 0;     // pure virtual: implemented by derived class

protected:
    RocdecParserParams parser_params_ = {};

    /*! \brief callback function pointers for the parser
     */
    PFNVIDSEQUENCECALLBACK pfn_sequece_cb_;             /**< Called before decoding frames and/or whenever there is a fmt change */
    PFNVIDDECODECALLBACK pfn_decode_picture_cb_;        /**< Called when a picture is ready to be decoded (decode order)         */
    PFNVIDDISPLAYCALLBACK pfn_display_picture_cb_;      /**< Called whenever a picture is ready to be displayed (display order)  */
    PFNVIDSEIMSGCALLBACK pfn_get_sei_message_cb_;       /**< Called when all SEI messages are parsed for particular frame        */

    uint32_t pic_count_;  // decoded picture count for the current bitstream
    uint32_t pic_width_;
    uint32_t pic_height_;
    bool new_sps_activated_;

    Rational frame_rate_;

    RocdecVideoFormat video_format_params_;
    RocdecSeiMessageInfo sei_message_info_params_;
    RocdecPicParams dec_pic_params_;

    // Picture bit stream info
    uint8_t *pic_data_buffer_ptr_;  // bit stream buffer pointer of the current frame from the demuxer
    int pic_data_size_;             // bit stream size of the current frame
    int curr_byte_offset_;            // current parsing byte offset

    // NAL unit info
    int start_code_num_;              // number of start codes found so far
    int curr_start_code_offset_;
    int next_start_code_offset_;
    int nal_unit_size_;

    int                 rbsp_size_;
    uint8_t             rbsp_buf_[RBSP_BUF_SIZE]; // to store parameter set or slice header RBSP

    int                 slice_num_;
    uint8_t*            pic_stream_data_ptr_;
    int                 pic_stream_data_size_;

    uint8_t             *sei_rbsp_buf_; // buffer to store SEI RBSP. Allocated at run time.
    uint32_t            sei_rbsp_buf_size_;
    std::vector<RocdecSeiMessage> sei_message_list_;
    int                 sei_message_count_;  // total SEI playload message count of the current frame.
    uint8_t             *sei_payload_buf_;  // buffer to store SEI playload. Allocated at run time.
    uint32_t            sei_payload_buf_size_;
    uint32_t            sei_payload_size_;  // total SEI payload size of the current frame

    /*! \brief Function to get the NAL Unit data
     * \return Returns OK if successful, else error code
     */
    ParserResult GetNalUnit();

    /*! \brief Function to convert from Encapsulated Byte Sequence Packets to Raw Byte Sequence Payload
     * 
     * \param [inout] stream_buffer A pointer of <tt>uint8_t</tt> for the converted RBSP buffer.
     * \param [in] begin_bytepos Start position in the EBSP buffer to convert
     * \param [in] end_bytepos End position in the EBSP buffer to convert, generally it's size.
     * \return Returns the size of the converted buffer in <tt>size_t</tt>
     */
    size_t EbspToRbsp(uint8_t *stream_buffer, size_t begin_bytepos, size_t end_bytepos);
};

// helpers
namespace Parser {
    inline char GetLowByte(uint16_t data) {
        return (data >> 8);
    }

    inline char GetHiByte(uint16_t data) {
        return (data & 0xFF);
    }

    inline bool GetBit(const uint8_t *data, size_t &bit_idx) {
        bool ret = (data[bit_idx / 8] >> (7 - bit_idx % 8) & 1);
        bit_idx++;
        return ret;
    }
    inline uint32_t GetBitToUint32(const uint8_t *data, size_t &bit_idx) {
        uint32_t ret = (data[bit_idx / 8] >> (7 - bit_idx % 8) & 1);
        bit_idx++;
        return ret;
    }

    inline uint32_t ReadBits(const uint8_t *data, size_t &start_bit_idx, size_t bits_to_read) {
        if (bits_to_read > 32) {
            return 0; // assert(0);
        }
        uint32_t result = 0;
        for (size_t i = 0; i < bits_to_read; i++) {
            result = result << 1;
            result |= GetBitToUint32(data, start_bit_idx); // start_bit_idx incremented inside
        }
        return result;
    }

    inline size_t CountContiniusZeroBits(const uint8_t *data, size_t &start_bit_idx) {
        size_t start_bit_idx_org = start_bit_idx;
        while (GetBit(data, start_bit_idx) == false) {} // start_bit_idx incremented inside
        start_bit_idx--; // remove non zero
        return start_bit_idx - start_bit_idx_org;
    }

    namespace ExpGolomb {
        inline uint32_t ReadUe(const uint8_t *data, size_t &start_bit_idx) {
            size_t zero_bits_count = CountContiniusZeroBits(data, start_bit_idx); // start_bit_idx incremented inside
            if (zero_bits_count > 30) {
                return 0; // assert(0)
            }

            uint32_t left_part = (0x1 << zero_bits_count) - 1;
            start_bit_idx++;
            uint32_t rightPart = ReadBits(data, start_bit_idx, zero_bits_count);
            return left_part + rightPart;
        }

        inline uint32_t ReadSe(const uint8_t *data, size_t &start_bit_idx) {
            uint32_t ue = ReadUe(data, start_bit_idx);
            // se From Ue 
            uint32_t mod2 = ue % 2;
            uint32_t r = ue / 2 + mod2;

            if (mod2 == 0) {
                return r * -1;
            }
            return r;
        }
    }
}