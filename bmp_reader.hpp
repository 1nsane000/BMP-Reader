#pragma once

#include <iostream>
#include <stdint.h>
#include <iterator>
#include <vector>
#include <fstream>
#include <string>
#include <sstream>
#include <memory>
#include <math.h>
#include <bitset>

static constexpr int MINIMUM_BMP_SIZE = 512;
static constexpr int IMAGE_DATA_OFFSET_OFFSET = 10;
static constexpr int BITMAP_HEADER_SIZE = 14;
static constexpr int DIB_HEADER_SIZE_OFFSET = 14;
static constexpr int WIDTH_OFFSET = 18;
static constexpr int HEIGHT_OFFSET = 22;
static constexpr int BITS_PER_PIXEL_OFFSET = 28;
static constexpr int COMPRESSION_METHOD_OFFSET = 30;
static constexpr int PIXARAY_SIZE_OFFSET = 34;
static constexpr int COLOR_PALLETE_OFFSET = 46;
static constexpr int HALFTONING_OFFSET = 60;

static constexpr int DIB_BITMAPINFOHEADER = 40;
static constexpr int DIB_BITMAPV5HEADER = 124;


static constexpr int BI_RGB = 0;
static constexpr int BI_RLE8 = 1;
static constexpr int BI_RLE4 = 2;
static constexpr int BI_BITFIELDS = 3;
static constexpr int BI_JPEG = 4;
static constexpr int BI_PNG = 5;
static constexpr int BI_ALPHABITFIELDS = 6;
static constexpr int BI_CMYK = 11;
static constexpr int BI_CMYKRLE8 = 12;
static constexpr int BI_CMYKRLE4 = 13;


struct RGB_color{
    RGB_color(uint8_t _r, uint8_t _g, uint8_t _b) : r(_r), g(_g), b(_b), a(255) {}
    RGB_color(uint8_t _r, uint8_t _g, uint8_t _b, uint8_t _a) : r(_r), g(_g), b(_b), a(_a) {}
    //RGB_color(const RGB_color& _c) : r(_c.r), g(_c.g), b(_c.b) {}
    RGB_color() : r(0), g(0), b(0),a(0) {}
    uint8_t r,g,b,a;
};

std::ostream& operator<<(std::ostream &strm, const RGB_color &a) {
    return strm << "R: " << static_cast<int>(a.r) << " G: " << static_cast<int>(a.g) << " B: "<< static_cast<int>(a.b) <<'\n';
}

class bmp_reader{
public:
    bmp_reader(const std::string& _path) : path(_path) {
        std::clog << path << '\n';
        load_image();
    }

    void output_to_ppm(std::ostream &out){
        if(loaded){
            std::stringstream str;
            str << "P3\n" << width << ' ' << height << "\n256\n";
            for(int i = 0; i< height;i++){
                for(int j = 0; j < width; j++){
                    write_color(str, pixel_map[i*width+j]);
                }
            }
            out << str.str();
        }
        else{
            std::clog << "Cannot output file since image failed to load\n";
        }

    }



private:
    std::string path;
    int width, height;
    uint64_t file_size;
    std::vector<RGB_color> pixel_map;
    std::vector<RGB_color> color_table;

    uint32_t image_data_offset;
    uint32_t compression_method;
    uint32_t color_pallete_colors;
    uint32_t DIB_header_size;
    uint32_t size_in_bytes;
    uint16_t bits_per_pixel;
    uint32_t calculated_image_data_size;

    //Used for 32 and 16 bit sampled images
    uint32_t r_mask;
    uint8_t r_shift;
    uint32_t g_mask;
    uint8_t g_shift;
    uint32_t b_mask;
    uint8_t b_shift;

    bool loaded = false;
    bool topdown = false;

    void write_color(std::ostream &out, RGB_color pixel_color){
        out << static_cast<int>(pixel_color.r) << ' '
            << static_cast<int>(pixel_color.g) << ' '
            << static_cast<int>(pixel_color.b) << '\n';
    }

    void load_image() {
        std::ifstream img_file(path, std::ios::binary);

        if(!img_file.is_open()){
            std::cerr << "Error opening file: " << path << '\n';
            return;
        }

        img_file.seekg(0, std::ios::end);
        file_size = img_file.tellg();
        img_file.seekg(0, std::ios::beg);

        char* buffer = (char*)malloc(file_size);
        img_file.read(buffer, file_size);
        img_file.close();

        std::string MB(buffer, 2);

        //Magic Byte check
        if (MB != "BM"
            && MB != "BA"
            && MB != "CI"
            && MB != "CP"
            && MB != "IC"
            && MB != "PT"
        ){
            std::cerr << "Invalid File Signature: " << MB << '\n';
            free(buffer);
            return;
        }
        std::copy(&(buffer[DIB_HEADER_SIZE_OFFSET]), &(buffer[DIB_HEADER_SIZE_OFFSET]) + sizeof(uint32_t), reinterpret_cast<char*>(&DIB_header_size));
        std::clog << "Header Size: " << DIB_header_size <<'\n';

        loaded = load_BITMAPV5HEADER(buffer);
        free(buffer);
    }

    bool load_BITMAPV5HEADER(char* buffer){
        get_header_data(buffer);
        calculated_image_data_size = file_size - image_data_offset;
        std::clog << "Calculated Image Data Size: " << calculated_image_data_size <<'\n';
        bool ret = true;
        if(abs(height) > 32727 || height == 0||width > 32727 || width <=0){
            ret = false;
        }
        else if(bits_per_pixel == 32){
            ret = read_32bit(buffer);
        }
        else if(bits_per_pixel == 24){
            ret = read_24bit(buffer);
        }
        else if(bits_per_pixel == 16){
            ret = read_16bit(buffer);
        }
        else if(bits_per_pixel == 8){
            ret = read_8bit(buffer);
        }
        else if(bits_per_pixel == 4){
            ret = read_4bit(buffer);
        }
        else if(bits_per_pixel == 1){
            ret = read_1bit(buffer);
        }
        else{
            std::clog << "Unsupported color depth" << '\n';
            ret = false;
        }
        if(!topdown && ret){
            reverse_rows();
        }
        return ret;
    }

    bool read_32bit(char* buffer){
        get_bitfield_mask(buffer);
        pixel_map.reserve(width+height);
        int row_size = static_cast<int>(ceil((bits_per_pixel * width)/32.0))  * 4;
        for(int i = 0; i < abs(height); i++){
            for(int j = 0; j < width*4 ; j+=4){
                int offset = image_data_offset + i*row_size + j;
                uint32_t ddword;
                std::copy(&(buffer[offset]), &(buffer[offset]) + sizeof(uint32_t), reinterpret_cast<char*>(&ddword));
                uint8_t b = (ddword & b_mask) >> b_shift;
                uint8_t g = (ddword & g_mask) >> g_shift;
                uint8_t r = (ddword & r_mask) >> r_shift;
                uint8_t a = (ddword & (~(r_mask|b_mask|g_mask))) >> 24; //This doesn't work
                pixel_map.push_back(RGB_color(r, g, b, a));
            }
        }
        return true;
    }

    bool read_24bit(char* buffer){
        pixel_map.reserve(width+height);
        int row_size = static_cast<int>(ceil((bits_per_pixel * width)/32.0))  * 4;
        for(int i = 0; i < abs(height); i++){
            for(int j = 0; j < width*3 ; j+=3){
                int offset = image_data_offset + i*row_size + j;
                uint8_t b = buffer[offset];
                uint8_t g = buffer[offset + 1];
                uint8_t r = buffer[offset + 2];
                pixel_map.push_back(RGB_color(r, g, b));
            }
        }
        return true;
    }

    bool read_16bit(char* buffer){
        get_bitfield_mask(buffer);

        int b_shiftback = 8 - std::bitset<8>(b_mask>>b_shift).count();
        int g_shiftback = 8 - std::bitset<8>(g_mask>>g_shift).count();
        int r_shiftback = 8 - std::bitset<8>(r_mask>>r_shift).count();
        std::clog << b_shiftback << ' ' << g_shiftback << ' ' << r_shiftback << '\n';
        pixel_map.reserve(width+height);

        int row_size = static_cast<int>(ceil((bits_per_pixel * width)/32.0))  * 4;
        for(int i = 0; i < abs(height); i++){
            for(int j = 0; j < width*2 ; j+=2){
                int offset = image_data_offset + i*row_size + j;
                uint16_t word;
                std::copy(&(buffer[offset]), &(buffer[offset]) + sizeof(uint16_t), reinterpret_cast<char*>(&word));
                uint8_t b = ((word & b_mask) >> b_shift) << b_shiftback;
                uint8_t g = ((word & g_mask) >> g_shift) << g_shiftback;
                uint8_t r = ((word & r_mask) >> r_shift) << r_shiftback;
                //std::clog << std::bitset<16>(word) << ' ' << std::bitset<8>(b) << ' ' << std::bitset<8>(g) << ' ' <<  std::bitset<8>(r) << '\n';

                if((b == (((b_mask >> b_shift) << b_shiftback)) &&
                    (g == ((g_mask >> g_shift) << g_shiftback)) &&
                    (r == ((r_mask >> r_shift) << r_shiftback)))){
                    //Convert maximum RBG values to pure white
                    //ie for default 16 bit RBG555 the maximum value is (248,248,248)
                    //which gets converted to (255,255,255)
                    r = 255;
                    b = 255;
                    g = 255;
                }
                pixel_map.push_back(RGB_color(r, g, b));
            }
        }
        return true;
    }

    bool read_8bit(char* buffer){
        if(!get_color_table_info(buffer)){
            return false;
        }
        if(compression_method == BI_RLE8){
            read_rle8(buffer);
        }
        else{
            read_8bit_standard(buffer);
        }
        return true;
    }

    bool read_8bit_standard(char* buffer){
        int row_size = static_cast<int>(ceil((bits_per_pixel * width)/32.0))  * 4;
        for(int i = 0; i < abs(height); i++){
            for(int j = 0; j < width ; j++){
                int offset = image_data_offset + i*row_size + j;
                uint8_t color_idx = buffer[offset];
                pixel_map.push_back(RGB_color(color_table[color_idx]));
            }
        }
        return true;
    }

    bool read_4bit(char* buffer){
        if(!get_color_table_info(buffer)){
            return false;
        }
        if(compression_method == BI_RLE4){
            read_rle4(buffer);
        }
        else{
            read_4bit_standard(buffer);
        }
        return true;
    }

    bool read_4bit_standard(char* buffer){
        int row_size = static_cast<int>(ceil((bits_per_pixel * width)/32.0))  * 4;
        for(int i = 0; i < abs(height); i++){
            int current_row_pixel_position = 0;
            for(int j = 0; current_row_pixel_position < width ; j++){
                int offset = image_data_offset + (i*row_size) + j;
                uint16_t value;
                std::copy(&(buffer[offset]), &(buffer[offset]) + sizeof(uint16_t), reinterpret_cast<char*>(&value));
                int color_idx = (value & 0xF0) >> 4;
                pixel_map.push_back(RGB_color(color_table[color_idx]));
                current_row_pixel_position++;
                if(current_row_pixel_position < width){
                    color_idx = (value & 0x0F);
                    pixel_map.push_back(RGB_color(color_table[color_idx]));
                    current_row_pixel_position++;
                }
            }
        }
        return true;
    }

    bool read_1bit(char* buffer){
        if(!get_color_table_info(buffer)){
            return false;
        }
        int row_size = static_cast<int>(ceil((bits_per_pixel * width)/32.0))  * 4;
        for(int i = 0; i < abs(height); i++){
            int current_row_pixel_position = 0;
            for(int j = 0; current_row_pixel_position < width ; j++){
                int offset = image_data_offset + (i*row_size) + j;
                uint8_t value = buffer[offset];
                for(int k = 0; k<8 && current_row_pixel_position < width; k++){
                    int color_idx = ((value & 0x80) >> 7);
                    value <<= 1;
                    pixel_map.push_back(RGB_color(color_table[color_idx]));
                    current_row_pixel_position++;
                }

            }
        }
        return true;
    }

    enum rle_state{
        INITIAL,
        ZERO_BYTE,
        ENCODED,
        ABSOLUTE,
        END
    };

    bool read_rle8(char* buffer){
        rle_state state = rle_state::INITIAL;
        uint32_t bitmap_pos = 0;
        int number_of_pixels = 0;
        int total_pixels = 0;
        while(bitmap_pos < size_in_bytes && state != rle_state::END){
            uint8_t byte = buffer[bitmap_pos + image_data_offset];
            switch(state){
                case rle_state::INITIAL: {
                    if(byte==0){
                        state = rle_state::ZERO_BYTE;
                    }
                    else{
                        state = rle_state::ENCODED;
                        number_of_pixels = byte;
                    }
                    break;
                }
                case rle_state::ZERO_BYTE: {
                    if(byte == 0){
                        //End of Line
                        state = rle_state::INITIAL;
                    }
                    else if(byte == 1){
                        //End of Bitmap
                        state = rle_state::END;
                    }
                    else if(byte == 2){
                        //Delta
                        state = rle_state::INITIAL;
                    }
                    else{
                        //In all other cases use absolute mode
                        number_of_pixels = byte;
                        total_pixels = byte;
                        state = rle_state::ABSOLUTE;
                    }
                    break;
                }
                case rle_state::ENCODED: {
                    RGB_color c = RGB_color(color_table[byte]);
                    for(int i = 0; i<number_of_pixels; i++){
                        pixel_map.push_back(c);
                    }
                    state = rle_state::INITIAL;
                    break;
                }
                case rle_state::ABSOLUTE: {
                    RGB_color c = RGB_color(color_table[byte]);
                    pixel_map.push_back(c);
                    number_of_pixels--;
                    if(number_of_pixels == 0){
                        if(total_pixels%2 != 0) {
                            //Adjust the alignment to a 16 bit boundary
                            bitmap_pos++;
                        }
                        state = rle_state::INITIAL;
                    }
                    break;
                }
                default: break;
            }
            bitmap_pos++;

        }
        return true;
    }

    bool read_rle4(char* buffer){
        rle_state state = rle_state::INITIAL;
        uint32_t bitmap_pos = 0;
        int number_of_pixels = 0;
        int total_pixels = 0;
        while(bitmap_pos < size_in_bytes && state != rle_state::END){
            uint8_t byte = buffer[bitmap_pos + image_data_offset];
            switch(state){
                case rle_state::INITIAL: {
                    if(byte==0){
                        state = rle_state::ZERO_BYTE;
                    }
                    else{
                        state = rle_state::ENCODED;
                        number_of_pixels = byte;
                    }
                    break;
                }
                case rle_state::ZERO_BYTE: {
                    if(byte == 0){
                        //End of Line
                        state = rle_state::INITIAL;
                    }
                    else if(byte == 1){
                        //End of Bitmap
                        state = rle_state::END;
                    }
                    else if(byte == 2){
                        //Delta
                        //Should probably skip pixels instead of reverting to the Initial state
                        state = rle_state::INITIAL;
                    }
                    else{
                        //In all other cases use absolute mode
                        number_of_pixels = byte;
                        total_pixels = byte;
                        state = rle_state::ABSOLUTE;
                    }
                    break;
                }
                case rle_state::ENCODED: {
                    uint8_t upper_index = (byte & 0xf0) >> 4;
                    uint8_t lower_index = byte & 0x0f;
                    RGB_color c1 = RGB_color(color_table[upper_index]);
                    RGB_color c2 = RGB_color(color_table[lower_index]);
                    for(int i = 0; i<number_of_pixels; i++){
                        if(i%2 == 0){
                            pixel_map.push_back(c1);
                        }
                        else{
                            pixel_map.push_back(c2);
                        }
                    }
                    state = rle_state::INITIAL;
                    break;
                }
                case rle_state::ABSOLUTE: {
                    uint8_t upper_index = (byte & 0xf0) >> 4;
                    RGB_color c = RGB_color(color_table[upper_index]);
                    pixel_map.push_back(c);
                    number_of_pixels--;

                    if(number_of_pixels > 0){
                        uint8_t lower_index = byte & 0x0f;
                        c = RGB_color(color_table[lower_index]);
                        pixel_map.push_back(c);
                        number_of_pixels--;
                    }
                    if(number_of_pixels == 0){
                        if(total_pixels%4 != 0 && total_pixels%4 != 3) {
                            //Adjust the alignment to a 16 bit boundary
                            bitmap_pos++;
                        }
                        state = rle_state::INITIAL;
                    }
                    break;
                }
                default: break;
            }
            bitmap_pos++;

        }
        return true;
    }



    void reverse_rows() {
        std::vector<RGB_color> reversed;
        reversed.resize(height * width);
        for(int i = abs(height)-1; i>=0; i--){
            int reversed_index = (abs(height) - 1 - i) * width;
            std::copy(pixel_map.begin()+i*width,pixel_map.begin()+i*width + width, reversed.begin() + reversed_index);
        }
        pixel_map = reversed;
    }

    bool get_color_table_info(char* buffer){
        if(color_pallete_colors > 256){
            return false;
        }
        int compression_additional_offset = 0;
        if(compression_method == BI_BITFIELDS){
            compression_additional_offset = 12;
        }
        else if(compression_method == BI_ALPHABITFIELDS){
            compression_additional_offset = 16;
        }

        int color_table_offset = DIB_header_size + DIB_HEADER_SIZE_OFFSET + compression_additional_offset;
        for(uint32_t i = 0; i< color_pallete_colors*4; i+=4){
            uint8_t b = buffer[color_table_offset+i];
            uint8_t g = buffer[color_table_offset+i+1];
            uint8_t r = buffer[color_table_offset+i+2];
            //int a = buffer[color_table_offset+i+3];
            color_table.push_back(RGB_color(r,g,b));
        }
        return true;
    }


    //Gets the masks which specify how the RGB colors are ordered
    //within the 32 bit unit which defines the pixel data
    void get_bitfield_mask(char* buffer){
        if(compression_method == BI_BITFIELDS){
            int current_pos = 0;
            std::copy(&(buffer[BITMAP_HEADER_SIZE + DIB_header_size + current_pos]), &(buffer[BITMAP_HEADER_SIZE + DIB_header_size + current_pos]) + sizeof(uint32_t), reinterpret_cast<char*>(&r_mask));
            current_pos+=4;
            std::copy(&(buffer[BITMAP_HEADER_SIZE + DIB_header_size + current_pos]), &(buffer[BITMAP_HEADER_SIZE + DIB_header_size + current_pos]) + sizeof(uint32_t), reinterpret_cast<char*>(&g_mask));
            current_pos+=4;
            std::copy(&(buffer[BITMAP_HEADER_SIZE + DIB_header_size + current_pos]), &(buffer[BITMAP_HEADER_SIZE + DIB_header_size + current_pos]) + sizeof(uint32_t), reinterpret_cast<char*>(&b_mask));
            r_shift = trailing_u32b_zeroes(r_mask);
            g_shift = trailing_u32b_zeroes(g_mask);
            b_shift = trailing_u32b_zeroes(b_mask);
        }
        else if(bits_per_pixel == 16){
            //Default colorspace for 16 bit is is RGB555
            r_mask = 0b111110000000000;
            r_shift = 10;
            g_mask = 0b000001111100000;
            g_shift = 5;
            b_mask = 0b000000000011111;
            b_shift = 0;
        }
        else{
            //32 bit
            r_mask = 0x00ff0000;
            r_shift = 16;
            g_mask = 0x0000ff00;
            g_shift = 8;
            b_mask = 0x000000ff;
            b_shift = 0;
        }


    }

    //Returns the number of trailing zeroes from a DDWORD sized primitive
    uint8_t trailing_u32b_zeroes(uint32_t v) {
        uint8_t c;
        if (v) {
            v &= -v;
            c = 0;
            if (v & 0xAAAAAAAAu) c |= 1;
            if (v & 0xCCCCCCCCu) c |= 2;
            if (v & 0xF0F0F0F0u) c |= 4;
            if (v & 0xFF00FF00u) c |= 8;
            if (v & 0xFFFF0000u) c |= 16;
        }
        else c = 32;
        return c;
    }

    void get_header_data(char* buffer) {
        std::copy(&(buffer[WIDTH_OFFSET]), &(buffer[WIDTH_OFFSET]) + sizeof(int), reinterpret_cast<char*>(&width));
        std::copy(&(buffer[HEIGHT_OFFSET]), &(buffer[HEIGHT_OFFSET]) + sizeof(int), reinterpret_cast<char*>(&height));
        std::copy(&(buffer[BITS_PER_PIXEL_OFFSET]), &(buffer[BITS_PER_PIXEL_OFFSET]) + sizeof(uint16_t), reinterpret_cast<char*>(&bits_per_pixel));
        std::copy(&(buffer[IMAGE_DATA_OFFSET_OFFSET]), &(buffer[IMAGE_DATA_OFFSET_OFFSET]) + sizeof(uint32_t), reinterpret_cast<char*>(&image_data_offset));
        std::copy(&(buffer[COMPRESSION_METHOD_OFFSET]), &(buffer[COMPRESSION_METHOD_OFFSET]) + sizeof(uint32_t), reinterpret_cast<char*>(&compression_method));
        std::copy(&(buffer[COLOR_PALLETE_OFFSET]), &(buffer[COLOR_PALLETE_OFFSET]) + sizeof(uint32_t), reinterpret_cast<char*>(&color_pallete_colors));
        std::copy(&(buffer[PIXARAY_SIZE_OFFSET]), &(buffer[PIXARAY_SIZE_OFFSET]) + sizeof(uint32_t), reinterpret_cast<char*>(&size_in_bytes));

        if(height < 0){
            height = abs(height);
            topdown = true;
        }
        std::clog << "Width: " << width << '\n';
        std::clog << "Height: " << height << '\n';
        std::clog << "Bits Per Pixel: " << bits_per_pixel << '\n';
        std::clog << "Compression: " << compression_method << '\n';
        std::clog << "Collor Pallete: " << color_pallete_colors << '\n';
        std::clog << "Image data offset: " << image_data_offset<< '\n';
    }




};
