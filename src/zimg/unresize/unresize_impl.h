#pragma once

#ifndef ZIMG_UNRESIZE_UNRESIZE_IMPL_H_
#define ZIMG_UNRESIZE_UNRESIZE_IMPL_H_

#include <memory>
#include "graphengine/filter.h"
#include "bilinear.h"

namespace zimg {

enum class CPUClass;
enum class PixelType;

namespace unresize {

class UnresizeImplH : public graphengine::Filter {
protected:
	graphengine::FilterDescriptor m_desc;
	BilinearContext m_context;

	UnresizeImplH(const BilinearContext &context, unsigned width, unsigned height, PixelType type);
public:
	int version() const noexcept override { return VERSION; }

	const graphengine::FilterDescriptor &descriptor() const noexcept override { return m_desc; }

	pair_unsigned get_row_deps(unsigned i) const noexcept override;

	pair_unsigned get_col_deps(unsigned left, unsigned right) const noexcept override;

	void init_context(void *) const noexcept override {}
};

class UnresizeImplV : public graphengine::Filter {
protected:
	graphengine::FilterDescriptor m_desc;
	BilinearContext m_context;

	UnresizeImplV(const BilinearContext &context, unsigned width, unsigned height, PixelType type);
public:
	int version() const noexcept override { return VERSION; }

	const graphengine::FilterDescriptor &descriptor() const noexcept override { return m_desc; }

	pair_unsigned get_row_deps(unsigned i) const noexcept override;

	pair_unsigned get_col_deps(unsigned left, unsigned right) const noexcept override;

	void init_context(void *) const noexcept override {}
};

struct UnresizeImplBuilder {
	unsigned up_width;
	unsigned up_height;
	PixelType type;

#include "common/builder.h"
	BUILDER_MEMBER(bool, horizontal)
	BUILDER_MEMBER(unsigned, orig_dim)
	BUILDER_MEMBER(double, shift)
	BUILDER_MEMBER(CPUClass, cpu)
#undef BUILDER_MEMBER

	UnresizeImplBuilder(unsigned up_width, unsigned up_height, PixelType type);

	std::unique_ptr<graphengine::Filter> create() const;
};

} // namespace unresize
} // namespace zimg

#endif // ZIMG_UNRESIZE_UNRESIZE_IMPL_H_
