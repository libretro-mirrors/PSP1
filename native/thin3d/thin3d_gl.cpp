#include <stdio.h>
#include <vector>
#include <string>
#include <map>

#include "base/logging.h"
#include "image/zim_load.h"
#include "math/lin/matrix4x4.h"
#include "thin3d/thin3d.h"
#include "gfx_es2/gl_state.h"
#include "gfx/gl_lost_manager.h"

static const unsigned short compToGL[] = {
	GL_NEVER,
	GL_LESS,
	GL_EQUAL,
	GL_LEQUAL,
	GL_GREATER,
	GL_NOTEQUAL,
	GL_GEQUAL,
	GL_ALWAYS
};

static const unsigned short blendEqToGL[] = {
	GL_FUNC_ADD,
	GL_FUNC_SUBTRACT,
	GL_FUNC_REVERSE_SUBTRACT,
};

static const unsigned short blendFactorToGL[] = {
	GL_ZERO,
	GL_ONE,
	GL_SRC_COLOR,
	GL_SRC_ALPHA,
	GL_ONE_MINUS_SRC_COLOR,
	GL_ONE_MINUS_SRC_ALPHA,
	GL_DST_COLOR,
	GL_DST_ALPHA,
	GL_ONE_MINUS_DST_COLOR,
	GL_ONE_MINUS_DST_ALPHA,
	GL_CONSTANT_COLOR,
};

#ifndef USING_GLES2
static const unsigned short logicOpToGL[] = {
	GL_CLEAR,
	GL_SET,
	GL_COPY,
	GL_COPY_INVERTED,
	GL_NOOP,
	GL_INVERT,
	GL_AND,
	GL_NAND,
	GL_OR,
	GL_NOR,
	GL_XOR,
	GL_EQUIV,
	GL_AND_REVERSE,
	GL_AND_INVERTED,
	GL_OR_REVERSE,
	GL_OR_INVERTED,
};
#endif

static const unsigned short primToGL[] = {
	GL_POINTS,
	GL_LINES,
	GL_TRIANGLES,
};

static const char *glsl_fragment_prelude =
"#ifdef GL_ES\n"
"precision mediump float;\n"
"#endif\n";

static inline void Uint32ToFloat4(uint32_t u, float f[4]) {
	f[0] = ((u >> 0) & 0xFF) * (1.0f / 255.0f);
	f[1] = ((u >> 8) & 0xFF) * (1.0f / 255.0f);
	f[2] = ((u >> 16) & 0xFF) * (1.0f / 255.0f);
	f[3] = ((u >> 24) & 0xFF) * (1.0f / 255.0f);
}

class Thin3DGLBlendState : public Thin3DBlendState {
public:
	bool enabled;
	GLuint eqCol, eqAlpha;
	GLuint srcCol, srcAlpha, dstCol, dstAlpha;
	bool logicEnabled;
	GLuint logicOp;
	// int maskBits;
	// uint32_t fixedColor;

	void Apply() {
		glstate.blend.set(enabled);
		glstate.blendEquationSeparate.set(eqCol, eqAlpha);
		glstate.blendFuncSeparate.set(srcCol, dstCol, srcAlpha, dstAlpha);
		glstate.colorMask.set(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		// glstate.blendColor.set(fixedColor);

#if !defined(USING_GLES2)
		glstate.colorLogicOp.set(logicEnabled);
		if (logicEnabled) {
			glstate.logicOp.set(logicOp);
		}
#endif

		// glstate.colorMask.set(maskBits & 1, (maskBits >> 1) & 1, (maskBits >> 2) & 1, (maskBits >> 3) & 1);
	}
};

class Thin3DGLDepthStencilState : public Thin3DDepthStencilState {
public:
	bool depthTestEnabled;
	bool depthWriteEnabled;
	GLuint depthComp;
	// bool stencilTestEnabled; TODO

	void Apply() {
		glstate.depthTest.set(depthTestEnabled);
		glstate.depthFunc.set(depthComp);
		glstate.depthWrite.set(depthWriteEnabled);
		glstate.stencilTest.disable();
	}
};

class Thin3DGLBuffer : public Thin3DBuffer, GfxResourceHolder {
public:
	Thin3DGLBuffer(size_t size, uint32_t flags) {
		glGenBuffers(1, &buffer_);
		target_ = (flags & T3DBufferUsage::INDEXDATA) ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER;
		usage_ = 0;
		if (flags & T3DBufferUsage::DYNAMIC)
			usage_ = GL_STREAM_DRAW;
		else
			usage_ = GL_STATIC_DRAW;
		knownSize_ = 0;
		register_gl_resource_holder(this);
	}
	~Thin3DGLBuffer() override {
		unregister_gl_resource_holder(this);
		glDeleteBuffers(1, &buffer_);
	}

	void SetData(const uint8_t *data, size_t size) override {
		Bind();
		glBufferData(target_, size, data, usage_);
		knownSize_ = size;
	}

	void SubData(const uint8_t *data, size_t offset, size_t size) override {
		Bind();
		if (size > knownSize_) {
			// Allocate the buffer.
			glBufferData(target_, size + offset, NULL, usage_);
			knownSize_ = size + offset;
		}
		glBufferSubData(target_, offset, size, data);
	}
	void Bind() {
		if (target_ == GL_ARRAY_BUFFER) {
			glstate.arrayBuffer.bind(buffer_);
		} else {
			glstate.elementArrayBuffer.bind(buffer_);
		}
	}

	void GLLost() override {
		ILOG("Recreating vertex buffer after glLost");
		knownSize_ = 0;  // Will cause a new glBufferData call. Should genBuffers again though?
		glGenBuffers(1, &buffer_);
	}

private:
	GLuint buffer_;
	GLuint target_;
	GLuint usage_;

	size_t knownSize_;
};

// Not registering this as a resource holder, instead ShaderSet is registered. It will
// invoke Compile again to recreate the shader then link them together.
class Thin3DGLShader : public Thin3DShader {
public:
	Thin3DGLShader(bool isFragmentShader) : shader_(0), type_(0) {
		type_ = isFragmentShader ? GL_FRAGMENT_SHADER : GL_VERTEX_SHADER;
	}

	bool Compile(const char *source);
	GLuint GetShader() const {
		return shader_;
	}
	const std::string &GetSource() const { return source_; }

	~Thin3DGLShader() {
		glDeleteShader(shader_);
	}

private:
	GLuint shader_;
	GLuint type_;
	bool ok_;
	std::string source_;  // So we can recompile in case of context loss.
};

bool Thin3DGLShader::Compile(const char *source) {
	source_ = source;
	shader_ = glCreateShader(type_);

	std::string temp;
	// Add the prelude on automatically for fragment shaders.
	if (type_ == GL_FRAGMENT_SHADER) {
		temp = std::string(glsl_fragment_prelude) + source;
		source = temp.c_str();
	}

	glShaderSource(shader_, 1, &source, 0);
	glCompileShader(shader_);
	GLint success = 0;
	glGetShaderiv(shader_, GL_COMPILE_STATUS, &success);
	if (!success) {
#define MAX_INFO_LOG_SIZE 2048
		GLchar infoLog[MAX_INFO_LOG_SIZE];
		GLsizei len = 0;
		glGetShaderInfoLog(shader_, MAX_INFO_LOG_SIZE, &len, infoLog);
		infoLog[len] = '\0';
		glDeleteShader(shader_);
		shader_ = 0;
		ILOG("%s Shader compile error:\n%s", type_ == GL_FRAGMENT_SHADER ? "Fragment" : "Vertex", infoLog);
	}
	ok_ = success != 0;
	return ok_;
}

class Thin3DGLVertexFormat : public Thin3DVertexFormat {
public:
	void Apply(const void *base = nullptr);
	void Unapply();
	void Compile();

	std::vector<Thin3DVertexComponent> components_;
	int semanticsMask_;  // Fast way to check what semantics to enable/disable.
	int stride_;
};

struct UniformInfo {
	int loc_;
};

// TODO: Fold BlendState into this? Seems likely to be right for DX12 etc.
// TODO: Add Uniform Buffer support.
class Thin3DGLShaderSet : public Thin3DShaderSet, GfxResourceHolder {
public:
	Thin3DGLShaderSet() {
		program_ = 0;
		register_gl_resource_holder(this);
	}
	~Thin3DGLShaderSet() {
		unregister_gl_resource_holder(this);
		vshader->Release();
		fshader->Release();
		glDeleteProgram(program_);
	}
	bool Link();
	
	void Apply();
	void Unapply();

	int GetUniformLoc(const char *name);

	void SetVector(const char *name, float *value, int n);
	void SetMatrix4x4(const char *name, const Matrix4x4 &value) override;

	void GLLost() {
		vshader->Compile(vshader->GetSource().c_str());
		fshader->Compile(fshader->GetSource().c_str());
		Link();
	}

	Thin3DGLShader *vshader;
	Thin3DGLShader *fshader;

private:
	GLuint program_;
	std::map<std::string, UniformInfo> uniforms_;
};

class Thin3DGLContext : public Thin3DContext {
public:
	Thin3DGLContext();
	virtual ~Thin3DGLContext();

	Thin3DDepthStencilState *CreateDepthStencilState(bool depthTestEnabled, bool depthWriteEnabled, T3DComparison depthCompare) override;
	Thin3DBlendState *CreateBlendState(const T3DBlendStateDesc &desc) override;
	Thin3DBuffer *CreateBuffer(size_t size, uint32_t usageFlags) override;
	Thin3DShaderSet *CreateShaderSet(Thin3DShader *vshader, Thin3DShader *fshader) override;
	Thin3DVertexFormat *CreateVertexFormat(const std::vector<Thin3DVertexComponent> &components, int stride, Thin3DShader *vshader) override;
	Thin3DTexture *CreateTexture(T3DTextureType type, T3DImageFormat format, int width, int height, int depth, int mipLevels) override;
	Thin3DTexture *CreateTexture() override;

	// Bound state objects
	void SetBlendState(Thin3DBlendState *state) override {
		Thin3DGLBlendState *s = static_cast<Thin3DGLBlendState *>(state);
		s->Apply();
	}

	// Bound state objects
	void SetDepthStencilState(Thin3DDepthStencilState *state) override {
		Thin3DGLDepthStencilState *s = static_cast<Thin3DGLDepthStencilState *>(state);
		s->Apply();
	}

	// The implementation makes the choice of which shader code to use.
	Thin3DShader *CreateVertexShader(const char *glsl_source, const char *hlsl_source);
	Thin3DShader *CreateFragmentShader(const char *glsl_source, const char *hlsl_source);

	void SetScissorEnabled(bool enable) override {
		glstate.scissorTest.set(enable);
	}

	void SetScissorRect(int left, int top, int width, int height) override {
		glstate.scissorRect.set(left, targetHeight_ - (top + height), width, height);
	}

	void SetViewports(int count, T3DViewport *viewports) override {
		// TODO: Add support for multiple viewports.
		glstate.viewport.set(viewports[0].TopLeftX, viewports[0].TopLeftY, viewports[0].Width, viewports[0].Height);
		glstate.depthRange.set(viewports[0].MinDepth, viewports[0].MaxDepth);
	}

	void SetTextures(int start, int count, Thin3DTexture **textures) override;

	void SetRenderState(T3DRenderState rs, uint32_t value) override;

	// TODO: Add more sophisticated draws.
	void Draw(T3DPrimitive prim, Thin3DShaderSet *shaderSet, Thin3DVertexFormat *format, Thin3DBuffer *vdata, int vertexCount, int offset) override;
	void DrawIndexed(T3DPrimitive prim, Thin3DShaderSet *shaderSet, Thin3DVertexFormat *format, Thin3DBuffer *vdata, Thin3DBuffer *idata, int vertexCount, int offset) override;
	void DrawUP(T3DPrimitive prim, Thin3DShaderSet *shaderSet, Thin3DVertexFormat *format, const void *vdata, int vertexCount) override;
	void Clear(int mask, uint32_t colorval, float depthVal, int stencilVal) override;

	const char *GetInfoString(T3DInfo info) const override {
		// TODO: Make these actually query the right information
		switch (info) {
			case APINAME:
	#ifdef USING_GLES2
				return "OpenGL ES";
	#else
				return "OpenGL";
	#endif
			case VENDORSTRING: return (const char *)glGetString(GL_VENDOR);
			case VENDOR:
				switch (gl_extensions.gpuVendor) {
				case GPU_VENDOR_AMD: return "VENDOR_AMD";
				case GPU_VENDOR_POWERVR: return "VENDOR_POWERVR";
				case GPU_VENDOR_NVIDIA: return "VENDOR_NVIDIA";
				case GPU_VENDOR_INTEL: return "VENDOR_INTEL";
				case GPU_VENDOR_ADRENO: return "VENDOR_ADRENO";
				case GPU_VENDOR_ARM: return "VENDOR_ARM";
				case GPU_VENDOR_BROADCOM: return "VENDOR_BROADCOM";
				case GPU_VENDOR_UNKNOWN:
				default:
					return "VENDOR_UNKNOWN";
				}
				break;
			case RENDERER: return (const char *)glGetString(GL_RENDERER);
			case SHADELANGVERSION: return (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
			case APIVERSION: return (const char *)glGetString(GL_VERSION);
			default: return "?";
		}
	}
};

Thin3DGLContext::Thin3DGLContext() {
	CreatePresets();
}

Thin3DGLContext::~Thin3DGLContext() {
}

Thin3DVertexFormat *Thin3DGLContext::CreateVertexFormat(const std::vector<Thin3DVertexComponent> &components, int stride, Thin3DShader *vshader) {
	Thin3DGLVertexFormat *fmt = new Thin3DGLVertexFormat();
	fmt->components_ = components;
	fmt->stride_ = stride;
	fmt->Compile();
	return fmt;
}

GLuint TypeToTarget(T3DTextureType type) {
	switch (type) {
#ifndef USING_GLES2
	case LINEAR1D: return GL_TEXTURE_1D;
#endif
	case LINEAR2D: return GL_TEXTURE_2D;
	case LINEAR3D: return GL_TEXTURE_3D;
	case CUBE: return GL_TEXTURE_CUBE_MAP;
#ifndef USING_GLES2
	case ARRAY1D: return GL_TEXTURE_1D_ARRAY;
#endif
	case ARRAY2D: return GL_TEXTURE_2D_ARRAY;
	default: return GL_NONE;
	}
}

class Thin3DGLTexture : public Thin3DTexture, GfxResourceHolder {
public:
	Thin3DGLTexture() : tex_(0), target_(0) {
		width_ = 0;
		height_ = 0;
		depth_ = 0;
		glGenTextures(1, &tex_);
		register_gl_resource_holder(this);
	}
	Thin3DGLTexture(T3DTextureType type, T3DImageFormat format, int width, int height, int depth, int mipLevels) : format_(format), tex_(0), target_(TypeToTarget(type)), mipLevels_(mipLevels) {
		width_ = width;
		height_ = height;
		depth_ = depth;
		glGenTextures(1, &tex_);
		register_gl_resource_holder(this);
	}
	~Thin3DGLTexture() {
		unregister_gl_resource_holder(this);
		Destroy();
	}

	bool Create(T3DTextureType type, T3DImageFormat format, int width, int height, int depth, int mipLevels) {
		format_ = format;
		target_ = TypeToTarget(type);
		mipLevels_ = mipLevels;
		width_ = width;
		height_ = height;
		depth_ = depth;
		return true;
	}

	void Destroy() {
		if (tex_) {
			glDeleteTextures(1, &tex_);
			tex_ = 0;
		}
	}
	void SetImageData(int x, int y, int z, int width, int height, int depth, int level, int stride, const uint8_t *data) override;
	void AutoGenMipmaps() override;

	void Bind() {
		glBindTexture(target_, tex_);
	}

	void GLLost() {
		if (!filename_.empty()) {
			if (LoadFromFile(filename_.c_str())) {
				ILOG("Reloaded lost texture %s", filename_.c_str());
			} else {
				ELOG("Failed to reload lost texture %s", filename_.c_str());
			}
		} else {
			WLOG("Texture %p cannot be restored - has no filename", this);
			tex_ = 0;
		}
	}
	void Finalize(int zim_flags);

private:
	GLuint tex_;
	GLuint target_;

	T3DImageFormat format_;
	int mipLevels_;
};

Thin3DTexture *Thin3DGLContext::CreateTexture() {
	return new Thin3DGLTexture();
}

Thin3DTexture *Thin3DGLContext::CreateTexture(T3DTextureType type, T3DImageFormat format, int width, int height, int depth, int mipLevels) {
	return new Thin3DGLTexture(type, format, width, height, depth, mipLevels);
}

void Thin3DGLTexture::AutoGenMipmaps() {
	Bind();
	glGenerateMipmap(target_);
}

void Thin3DGLTexture::SetImageData(int x, int y, int z, int width, int height, int depth, int level, int stride, const uint8_t *data) {
	int internalFormat;
	int format;
	int type;
	switch (format_) {
	case RGBA8888:
		internalFormat = GL_RGBA;
		format = GL_RGBA;
		type = GL_UNSIGNED_BYTE;
		break;
	case RGBA4444:
		internalFormat = GL_RGBA;
		format = GL_RGBA;
		type = GL_UNSIGNED_SHORT_4_4_4_4;
		break;
	default:
		return;
	}

	Bind();
	switch (target_) {
	case GL_TEXTURE_2D:
		glTexImage2D(GL_TEXTURE_2D, level, internalFormat, width_, height_, 0, format, type, data);
		break;
	default:
		ELOG("Thin3D GL: Targets other than GL_TEXTURE_2D not yet supported");
		break;
	}
}

bool isPowerOf2(int n) {
	return n == 1 || (n & (n - 1)) == 0;
}

void Thin3DGLTexture::Finalize(int zim_flags) {
	GLenum wrap = GL_REPEAT;
	if ((zim_flags & ZIM_CLAMP) || !isPowerOf2(width_) || !isPowerOf2(height_))
		wrap = GL_CLAMP_TO_EDGE;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	if ((zim_flags & (ZIM_HAS_MIPS | ZIM_GEN_MIPS))) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
	} else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
}


void Thin3DGLVertexFormat::Compile() {
	int sem = 0;
	for (int i = 0; i < (int)components_.size(); i++) {
		sem |= 1 << components_[i].semantic;
	}
	semanticsMask_ = sem;
	// TODO : Compute stride as well?
}

Thin3DDepthStencilState *Thin3DGLContext::CreateDepthStencilState(bool depthTestEnabled, bool depthWriteEnabled, T3DComparison depthCompare) {
	Thin3DGLDepthStencilState *ds = new Thin3DGLDepthStencilState();
	ds->depthTestEnabled = depthTestEnabled;
	ds->depthWriteEnabled = depthWriteEnabled;
	ds->depthComp = compToGL[depthCompare];
	return ds;
}

Thin3DBlendState *Thin3DGLContext::CreateBlendState(const T3DBlendStateDesc &desc) {
	Thin3DGLBlendState *bs = new Thin3DGLBlendState();
	bs->enabled = desc.enabled;
	bs->eqCol = blendEqToGL[desc.eqCol];
	bs->srcCol = blendFactorToGL[desc.srcCol];
	bs->dstCol = blendFactorToGL[desc.dstCol];
	bs->eqAlpha = blendEqToGL[desc.eqAlpha];
	bs->srcAlpha = blendFactorToGL[desc.srcAlpha];
	bs->dstAlpha = blendFactorToGL[desc.dstAlpha];
#ifndef USING_GLES2
	bs->logicEnabled = desc.logicEnabled;
	bs->logicOp = logicOpToGL[desc.logicOp];
#endif
	return bs;
}

Thin3DBuffer *Thin3DGLContext::CreateBuffer(size_t size, uint32_t usageFlags) {
	return new Thin3DGLBuffer(size, usageFlags);
}

Thin3DShaderSet *Thin3DGLContext::CreateShaderSet(Thin3DShader *vshader, Thin3DShader *fshader) {
	if (!vshader || !fshader) {
		ELOG("ShaderSet requires both a valid vertex and a fragment shader: %p %p", vshader, fshader);
		return NULL;
	}
	Thin3DGLShaderSet *shaderSet = new Thin3DGLShaderSet();
	vshader->AddRef();
	fshader->AddRef();
	shaderSet->vshader = static_cast<Thin3DGLShader *>(vshader);
	shaderSet->fshader = static_cast<Thin3DGLShader *>(fshader);
	if (shaderSet->Link()) {
		return shaderSet;
	} else {
		delete shaderSet;
		return NULL;
	}
}

void Thin3DGLContext::SetTextures(int start, int count, Thin3DTexture **textures) {
	for (int i = start; i < start + count; i++) {
		Thin3DGLTexture *glTex = static_cast<Thin3DGLTexture *>(textures[i]);
		glActiveTexture(GL_TEXTURE0 + i);
		glTex->Bind();
	}
	glActiveTexture(GL_TEXTURE0);
}


Thin3DShader *Thin3DGLContext::CreateVertexShader(const char *glsl_source, const char *hlsl_source) {
	Thin3DGLShader *shader = new Thin3DGLShader(false);
	if (shader->Compile(glsl_source)) {
		return shader;
	} else {
		shader->Release();
		return nullptr;
	}
}

Thin3DShader *Thin3DGLContext::CreateFragmentShader(const char *glsl_source, const char *hlsl_source) {
	Thin3DGLShader *shader = new Thin3DGLShader(true);
	if (shader->Compile(glsl_source)) {
		return shader;
	} else {
		shader->Release();
		return nullptr;
	}
}

bool Thin3DGLShaderSet::Link() {
	program_ = glCreateProgram();
	glAttachShader(program_, vshader->GetShader());
	glAttachShader(program_, fshader->GetShader());

	// Bind all the common vertex data points. Mismatching ones will be ignored.
	glBindAttribLocation(program_, SEM_POSITION, "Position");
	glBindAttribLocation(program_, SEM_COLOR0, "Color0");
	glBindAttribLocation(program_, SEM_TEXCOORD0, "TexCoord0");
	glBindAttribLocation(program_, SEM_NORMAL, "Normal");
	glBindAttribLocation(program_, SEM_TANGENT, "Tangent");
	glBindAttribLocation(program_, SEM_BINORMAL, "Binormal");
	glLinkProgram(program_);

	GLint linkStatus = GL_FALSE;
	glGetProgramiv(program_, GL_LINK_STATUS, &linkStatus);
	if (linkStatus != GL_TRUE) {
		GLint bufLength = 0;
		glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &bufLength);
		if (bufLength) {
			char* buf = new char[bufLength];
			glGetProgramInfoLog(program_, bufLength, NULL, buf);
			ELOG("Could not link program:\n %s", buf);
			// We've thrown out the source at this point. Might want to do something about that.
#ifdef _WIN32
			OutputDebugStringUTF8(buf);
#endif
			delete[] buf;
		}
		return false;
	}

	// Auto-initialize samplers.
	glUseProgram(program_);
	for (int i = 0; i < 4; i++) {
		char temp[256];
		sprintf(temp, "Sampler%i", i);
		int samplerLoc = GetUniformLoc(temp);
		if (samplerLoc != -1) {
			glUniform1i(samplerLoc, i);
		}
	}

	// Here we could (using glGetAttribLocation) save a bitmask about which pieces of vertex data are used in the shader
	// and then AND it with the vertex format bitmask later...
	return true;
}

int Thin3DGLShaderSet::GetUniformLoc(const char *name) {
	auto iter = uniforms_.find(name);
	int loc = -1;
	if (iter != uniforms_.end()) {
		loc = iter->second.loc_;
	} else {
		loc = glGetUniformLocation(program_, name);
		UniformInfo info;
		info.loc_ = loc;
		uniforms_[name] = info;
	}
	return loc;
}

void Thin3DGLShaderSet::SetVector(const char *name, float *value, int n) {
	glUseProgram(program_);
	int loc = GetUniformLoc(name);
	if (loc != -1) {
		switch (n) {
		case 1: glUniform1fv(loc, 1, value); break;
		case 2: glUniform1fv(loc, 2, value); break;
		case 3: glUniform1fv(loc, 3, value); break;
		case 4: glUniform1fv(loc, 4, value); break;
		}
	}
}

void Thin3DGLShaderSet::SetMatrix4x4(const char *name, const Matrix4x4 &value) {
	glUseProgram(program_);
	int loc = GetUniformLoc(name);
	if (loc != -1) {
		glUniformMatrix4fv(loc, 1, false, value.getReadPtr());
	}
}

void Thin3DGLShaderSet::Apply() {
	glUseProgram(program_);
}

void Thin3DGLShaderSet::Unapply() {
	glUseProgram(0);
}

void Thin3DGLContext::SetRenderState(T3DRenderState rs, uint32_t value) {
	switch (rs) {
	case T3DRenderState::CULL_MODE:
		switch (value) {
		case T3DCullMode::NO_CULL: glstate.cullFace.disable(); break;
		case T3DCullMode::CCW: glstate.cullFace.enable(); glstate.cullFaceMode.set(GL_CCW); break;
		case T3DCullMode::CW: glstate.cullFace.enable(); glstate.cullFaceMode.set(GL_CW); break;
		}
		break;
	}
}

void Thin3DGLContext::Draw(T3DPrimitive prim, Thin3DShaderSet *shaderSet, Thin3DVertexFormat *format, Thin3DBuffer *vdata, int vertexCount, int offset) {
	Thin3DGLShaderSet *ss = static_cast<Thin3DGLShaderSet *>(shaderSet);
	Thin3DGLBuffer *vbuf = static_cast<Thin3DGLBuffer *>(vdata);
	Thin3DGLVertexFormat *fmt = static_cast<Thin3DGLVertexFormat *>(format);

	vbuf->Bind();
	fmt->Apply();
	ss->Apply();

	glDrawArrays(primToGL[prim], offset, vertexCount);

	ss->Unapply();
	fmt->Unapply();
}

void Thin3DGLContext::DrawIndexed(T3DPrimitive prim, Thin3DShaderSet *shaderSet, Thin3DVertexFormat *format, Thin3DBuffer *vdata, Thin3DBuffer *idata, int vertexCount, int offset) {
	Thin3DGLShaderSet *ss = static_cast<Thin3DGLShaderSet *>(shaderSet);
	Thin3DGLBuffer *vbuf = static_cast<Thin3DGLBuffer *>(vdata);
	Thin3DGLBuffer *ibuf = static_cast<Thin3DGLBuffer *>(idata);
	Thin3DGLVertexFormat *fmt = static_cast<Thin3DGLVertexFormat *>(format);

	vbuf->Bind();
	ibuf->Bind();
	fmt->Apply();
	ss->Apply();
	
	glDrawElements(primToGL[prim], offset, GL_INT, 0);
	
	ss->Unapply();
	fmt->Unapply();
}

void Thin3DGLContext::DrawUP(T3DPrimitive prim, Thin3DShaderSet *shaderSet, Thin3DVertexFormat *format, const void *vdata, int vertexCount) {
	Thin3DGLShaderSet *ss = static_cast<Thin3DGLShaderSet *>(shaderSet);
	Thin3DGLVertexFormat *fmt = static_cast<Thin3DGLVertexFormat *>(format);

	fmt->Apply(vdata);
	ss->Apply();

	glDrawArrays(primToGL[prim], 0, vertexCount);

	ss->Unapply();
	fmt->Unapply();
}

void Thin3DGLContext::Clear(int mask, uint32_t colorval, float depthVal, int stencilVal) {
	float col[4];
	Uint32ToFloat4(colorval, col);
	GLuint glMask = 0;
	if (mask & T3DClear::COLOR) {
		glClearColor(col[0], col[1], col[2], col[3]);
		glMask |= GL_COLOR_BUFFER_BIT;
	}
	if (mask & T3DClear::DEPTH) {
#ifdef USING_GLES2
		glClearDepthf(depthVal);
#else
		glClearDepth(depthVal);
#endif
		glMask |= GL_DEPTH_BUFFER_BIT;
	}
	if (mask & T3DClear::STENCIL) {
		glClearStencil(stencilVal);
		glMask |= GL_STENCIL_BUFFER_BIT;
	}
	glClear(glMask);
}

Thin3DContext *T3DCreateGLContext() {
	return new Thin3DGLContext();
}

void Thin3DGLVertexFormat::Apply(const void *base) {
	for (int i = 0; i < SEM_MAX; i++) {
		if (semanticsMask_ & (1 << i)) {
			glEnableVertexAttribArray(i);
		}
	}
	intptr_t b = (intptr_t)base;
	for (size_t i = 0; i < components_.size(); i++) {
		switch (components_[i].type) {
		case FLOATx2:
			glVertexAttribPointer(components_[i].semantic, 2, GL_FLOAT, GL_FALSE, stride_, (void *)(b + (intptr_t)components_[i].offset));
			break;
		case FLOATx3:
			glVertexAttribPointer(components_[i].semantic, 3, GL_FLOAT, GL_FALSE, stride_, (void *)(b + (intptr_t)components_[i].offset));
			break;
		case FLOATx4:
			glVertexAttribPointer(components_[i].semantic, 4, GL_FLOAT, GL_FALSE, stride_, (void *)(b + (intptr_t)components_[i].offset));
			break;
		case UNORM8x4:
			glVertexAttribPointer(components_[i].semantic, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride_, (void *)(b + (intptr_t)components_[i].offset));
			break;
		case INVALID:
			ELOG("Thin3DGLVertexFormat: Invalid component type applied.");
		}
	}
}

void Thin3DGLVertexFormat::Unapply() {
	for (int i = 0; i < SEM_MAX; i++) {
		if (semanticsMask_ & (1 << i)) {
			glDisableVertexAttribArray(i);
		}
	}
}
