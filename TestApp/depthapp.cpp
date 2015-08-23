#include <cstring>
#include <iostream>
#include <iterator>
#include <string>
#include "Common/cpuinfo.h"
#include "Common/pixel.h"
#include "Common/plane.h"
#include "Common/static_map.h"
#include "Depth/depth2.h"
#include "apps.h"
#include "frame.h"
#include "utils.h"

using namespace zimg;

namespace {;

struct AppContext {
	const char *infile;
	const char *outfile;
	int width;
	int height;
	PixelType pixtype_in;
	PixelType pixtype_out;
	depth::DitherType dither;
	int bits_in;
	int bits_out;
	int fullrange_in;
	int fullrange_out;
	int yuv;
	const char *visualise;
	int times;
	CPUClass cpu;
};

int select_dither(const char **opt, const char **lastopt, void *p, void *user)
{
	static static_string_map<depth::DitherType, 4> map{
		{ "none",            depth::DitherType::DITHER_NONE },
		{ "ordered",         depth::DitherType::DITHER_ORDERED },
		{ "random",          depth::DitherType::DITHER_RANDOM },
		{ "error_diffusion", depth::DitherType::DITHER_ERROR_DIFFUSION },
	};

	AppContext *c = (AppContext *)p;

	if (lastopt - opt < 2)
		throw std::invalid_argument{ "insufficient arguments for option dither" };

	auto it = map.find(opt[1]);
	c->dither = (it == map.end()) ? throw std::invalid_argument{ "unsupported dither type" } : it->second;

	return 2;
}

const AppOption OPTIONS[] = {
	{ "dither",    OptionType::OPTION_SPECIAL,  0, select_dither },
	{ "bits-in",   OptionType::OPTION_INTEGER,  offsetof(AppContext, bits_in) },
	{ "bits-out",  OptionType::OPTION_INTEGER,  offsetof(AppContext, bits_out) },
	{ "tv-in",     OptionType::OPTION_FALSE,    offsetof(AppContext, fullrange_in) },
	{ "pc-in",     OptionType::OPTION_TRUE,     offsetof(AppContext, fullrange_in) },
	{ "tv-out",    OptionType::OPTION_FALSE,    offsetof(AppContext, fullrange_out) },
	{ "pc-out",    OptionType::OPTION_TRUE,     offsetof(AppContext, fullrange_out) },
	{ "yuv",       OptionType::OPTION_TRUE,     offsetof(AppContext, yuv) },
	{ "rgb",       OptionType::OPTION_FALSE,    offsetof(AppContext, yuv) },
	{ "visualise", OptionType::OPTION_STRING,   offsetof(AppContext, visualise) },
	{ "times",     OptionType::OPTION_INTEGER,  offsetof(AppContext, times) },
	{ "cpu",       OptionType::OPTION_CPUCLASS, offsetof(AppContext, cpu) },
};

void usage()
{
	std::cout << "depth infile outfile w h pxl_in pxl_out [--dither dither] [--bits-in bits] [--bits-out bits] [--tv-in | pc-in] [--tv-out | --pc-out] [--yuv | --rgb] [--visualise path] [--times n] [--cpu cpu]\n";
	std::cout << "    infile               input file\n";
	std::cout << "    outfile              output file\n";
	std::cout << "    w                    image width\n";
	std::cout << "    h                    image height\n";
	std::cout << "    pxl_in               input pixel type\n";
	std::cout << "    pxl_out              output pixel type\n";
	std::cout << "    --dither             select dithering type\n";
	std::cout << "    --bits-in            input bit depth (integer only)\n";
	std::cout << "    --bits-out           output bit depth (integer only)\n";
	std::cout << "    --tv-in | --pc-in    toggle TV vs PC range for input\n";
	std::cout << "    --tv-out | --pc-out  toggle TV vs PC range for output\n";
	std::cout << "    --yuv | --rgb        toggle YUV vs RGB\n";
	std::cout << "    --visualise          path to BMP file for visualisation\n";
	std::cout << "    --times              number of cycles\n";
	std::cout << "    --cpu                select CPU type\n";
}

void execute(const depth::Depth2 &depth, const depth::Depth2 &depth_uv, Frame &in, Frame &out, bool yuv, int times)
{
	auto tmp = alloc_filter_tmp(depth, in, out);
	auto tmp_uv = alloc_filter_tmp(depth_uv, in, out);

	measure_time(times, [&]()
	{
		for (int p = 0; p < 3; ++p) {
			const depth::Depth2 &depth_ctx = (p > 0 && yuv) ? depth_uv : depth;
			void *tmp_pool = (p > 0 && yuv) ? tmp_uv.data() : tmp.data();

			apply_filter(depth_ctx, in, out, tmp_pool, p);
		}
	});
}

void export_for_bmp(const Frame &in, Frame &out, PixelType type, int bits, bool fullrange, bool yuv)
{
	for (int p = 0; p < 3; ++p) {
		bool chroma = yuv && (p == 1 || p == 2);
		PixelFormat src_format{ type, bits, fullrange, chroma };
		PixelFormat dst_format{ PixelType::BYTE, 8, fullrange, chroma };

		depth::Depth2 depth{ depth::DitherType::DITHER_NONE, (unsigned)in.width(), (unsigned)in.height(), src_format, dst_format, CPUClass::CPU_NONE };

		auto tmp = alloc_filter_tmp(depth, in, out);
		apply_filter(depth, in, out, tmp.data(), p);
	}
}

} // namespace


int depth_main(int argc, const char **argv)
{
	if (argc < 7) {
		usage();
		return -1;
	}

	AppContext c{};

	c.infile        = argv[1];
	c.outfile       = argv[2];
	c.width         = std::stoi(argv[3]);
	c.height        = std::stoi(argv[4]);
	c.pixtype_in    = select_pixel_type(argv[5]);
	c.pixtype_out   = select_pixel_type(argv[6]);
	c.bits_in       = -1;
	c.bits_out      = -1;
	c.dither        = depth::DitherType::DITHER_NONE;
	c.fullrange_in  = 0;
	c.fullrange_out = 0;
	c.yuv           = 0;
	c.visualise     = nullptr;
	c.times         = 1;
	c.cpu           = CPUClass::CPU_NONE;

	parse_opts(argv + 7, argv + argc, std::begin(OPTIONS), std::end(OPTIONS), &c, nullptr);

	c.bits_in  = c.bits_in < 0 ? pixel_size(c.pixtype_in) * 8 : c.bits_in;
	c.bits_out = c.bits_out < 0 ? pixel_size(c.pixtype_out) * 8 : c.bits_out;

	int width = c.width;
	int height = c.height;

	Frame in{ width, height, pixel_size(c.pixtype_in), 3 };
	Frame out{ width, height, pixel_size(c.pixtype_out), 3 };

	PixelFormat pixel_in_y{ c.pixtype_in, c.bits_in, !!c.fullrange_in, false };
	PixelFormat pixel_out_y{ c.pixtype_out, c.bits_out, !!c.fullrange_out, false };
	PixelFormat pixel_in_uv{ c.pixtype_in, c.bits_in, !!c.fullrange_in, !!c.yuv };
	PixelFormat pixel_out_uv{ c.pixtype_out, c.bits_out, !!c.fullrange_out, !!c.yuv };

	read_frame_raw(in, c.infile);

	depth::Depth2 depth{ c.dither, (unsigned)width, (unsigned)height, pixel_in_y, pixel_out_y, c.cpu };
	depth::Depth2 depth_uv{ c.dither, (unsigned)width, (unsigned)height, pixel_in_uv, pixel_out_uv, c.cpu };
	execute(depth, depth_uv, in, out, !!c.yuv, c.times);

	write_frame_raw(out, c.outfile);

	if (c.visualise) {
		Frame bmp{ width, height, 1, 3 };

		export_for_bmp(out, bmp, c.pixtype_out, c.bits_out, !!c.fullrange_out, !!c.yuv);
		write_frame_bmp(bmp, c.visualise);
	}

	return 0;
}
