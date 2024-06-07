#include "Shader.h"
#include "OpenGL.h"
#include <rpp/file_io.h>
#include "DefaultShaders.h"

namespace AGL
{
    ////////////////////////////////////////////////////////////////////////////////

    VertexDescr::VertexDescr() noexcept : sizeOf{0}
    {
    }

    VertexDescr::VertexDescr(int sizeOf, ShaderAttr attr0, int size0, ...) noexcept : sizeOf{sizeOf}
    {
        items[0].attr = (uint8_t)attr0;
        items[0].size = (uint8_t)size0;
        va_list ap;	va_start(ap, size0);

        int offset = size0 * sizeof(float);
        int i = 1;
        for (; offset < sizeOf && i < 4; ++i) 
        {
            items[i].attr = va_arg(ap, int);         // attrib location
            items[i].size = va_arg(ap, int);         // attrib size in floats
            offset += items[i].size * sizeof(float); // offset is in bytes
        }
        for (; i < 4; ++i) { // clear other attributes for accurate memcmp
            items[i].attr = 0;
            items[i].size = 0;
        }
    #ifdef DEBUG
        validate(); // ensure this descriptor is actually valid
    #endif
    }

    void VertexDescr::validate() const
    {
        int offset = 0;
        for (int i = 0; offset < sizeOf && i < 4; ++i) 
        {
            ShaderAttr a = (ShaderAttr)items[i].attr; // attrib location
            const int  s = (const int) items[i].size; // attrib size in floats
            Assert(0 <= a && a < a_MaxAttributes, "Invalid attr: check vertex_descr!");
            Assert(1 <= s && s <= 4, "Invalid attr size: check vertex_descr!");
            Assert(s <= (sizeOf - offset)/(int)sizeof(float), "Invalid layout: vertex_descr sizeOf doesn't match total attr sizes!");
            offset += s * sizeof(float); // offset is in bytes
            (void)a;
        }
        Assert(offset == sizeOf, "Invalid layout: end offset does not match vertex_descr sizeOf!");
    }

    void VertexDescr::clear()
    {
        memset(this, 0, sizeof(*this));
    }

    ////////////////////////////////////////////////////////////////////////////////

    VertexBuffer::VertexBuffer() noexcept
    {
    }
    VertexBuffer::~VertexBuffer()
    {
        clear();
    }

    // EMSCRIPTEN does not support VertexArrayObjects;
    void VertexBuffer::enableAttribs()
    {
        for (int i = 0, off = 0; off < layout.sizeOf; ++i)
        {
            ShaderAttr a = (ShaderAttr)layout.items[i].attr; // attrib location
            const int  s = (const int) layout.items[i].size; // attrib size in floats
            glEnableVertexAttribArray(a);
            glVertexAttribPointer(a, s, GL_FLOAT, 0, layout.sizeOf, (void*)uintptr_t(off));
            off += s * sizeof(float); // offset is in bytes
        }
    }

    void VertexBuffer::disableAttribs()
    {
        for (int i = 0, off = 0; off < layout.sizeOf; ++i)
        {
            ShaderAttr a = (ShaderAttr)layout.items[i].attr; // attrib location
            const int  s = (const int)layout.items[i].size; // attrib size in floats
            glDisableVertexAttribArray(a);
            off += s * sizeof(float); // offset is in bytes
        }
    }

    void VertexBuffer::create(const void*    verts,   int numVerts,
                              const index_t* indices, int numIndices,
                              const VertexDescr& layout)
    {
        drawMode     = DrawIndexed;
        vertexCount  = numVerts;
        indexCount   = numIndices;
        this->layout = layout;

        if (vertexArray) glDeleteVertexArrays(1, &vertexArray);
        glGenVertexArrays(1, &vertexArray);
        glBindVertexArray(vertexArray);

        // create and fill index buffer
        uint32_t indexBuf;
        glGenBuffers(1, &indexBuf);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuf);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, numIndices*sizeof(index_t), indices, GL_STATIC_DRAW);

        // create & fill vertex buffer
        uint32_t vertexBuf;
        glGenBuffers(1, &vertexBuf);
        glBindBuffer(GL_ARRAY_BUFFER, vertexBuf);
        glBufferData(GL_ARRAY_BUFFER, numVerts*layout.sizeOf, verts, GL_STATIC_DRAW);

        enableAttribs();

        glBindVertexArray(0);
        glDeleteBuffers(1, &vertexBuf);
        glDeleteBuffers(1, &indexBuf);
    }

    void VertexBuffer::create(DrawMode mode, const void* verts, 
                              int numVerts, const VertexDescr& layout)
    {
        drawMode     = mode;
        vertexCount  = numVerts;
        indexCount   = 0;
        this->layout = layout;

        if (vertexArray) glDeleteVertexArrays(1, &vertexArray);
        glGenVertexArrays(1, &vertexArray);
        glBindVertexArray(vertexArray);

        // create & fill vertex buffer
        uint32_t vertexBuf;
        glGenBuffers(1, &vertexBuf);
        glBindBuffer(GL_ARRAY_BUFFER, vertexBuf);
        glBufferData(GL_ARRAY_BUFFER, numVerts*layout.sizeOf, verts, GL_STATIC_DRAW);

        enableAttribs();

        glBindVertexArray(0);
        glDeleteBuffers(1, &vertexBuf);
    }

    void VertexBuffer::draw() const
    {
        if (drawMode == DrawIndexed)
        {
            glBindVertexArray(vertexArray);
            glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
            glBindVertexArray(0);
        }
        else if (drawMode != DrawNone)
        {
            // map DrawMode: Triangles,TriangleStrip,Indexed; to OpenGL modes
            static constexpr GLenum modeMap[] = {
                GL_NONE, GL_NONE, GL_TRIANGLES, GL_TRIANGLE_STRIP,
                GL_LINES, GL_LINE_STRIP, GL_LINE_LOOP,
            };
            const GLenum mode = modeMap[drawMode];
            
            glBindVertexArray(vertexArray);
            glDrawArrays(mode, 0, vertexCount);
            glBindVertexArray(0);
        }
    }

    void VertexBuffer::clear()
    {
        if (vertexArray) glDeleteVertexArrays(1, &vertexArray), vertexArray = 0;
        vertexCount = indexCount = 0;
        drawMode = DrawNone;
    }

    bool VertexBuffer::hasAttrib(ShaderAttr shaderAttr) const
    {
        for (int i = 0, off = 0; off < layout.sizeOf; ++i)
        {
            if ((ShaderAttr)layout.items[i].attr == shaderAttr)
                return true;
            off += layout.items[i].size * sizeof(float); // offset is in bytes
        }
        return false;
    }

    ////////////////////////////////////////////////////////////////////////////////

    static const char* UniformMap[u_MaxUniforms] = {
        "transform",     // u_Transform
        "diffuseTex",    // u_DiffuseTex
        "specularTex",   // u_SpecularTex
        "normalTex",     // u_NormalTex
        "shadowTex",     // u_ShadowTex
        "occludeTex",    // u_OccludeTex
        "diffuseColor",  // u_DiffuseColor
        "outlineColor",  // u_OutlineColor
        "shaderData",    // u_ShaderData
    };
    static const char* AttributeMap[a_MaxAttributes] = {
        "position",      // a_Position
        "normal",        // a_Normal
        "coord",         // a_Coord
        "coord2",        // a_Coord2
        "vertex",        // a_Vertex
        "color",         // a_Color
    };

    static const char* uniform_name(ShaderUniform uniformSlot) {
        if (0 <= uniformSlot && uniformSlot < u_MaxUniforms)
            return UniformMap[uniformSlot];
        return "u_invalid";
    }

    static int glProgramInt(GLuint program, GLenum propertyName) {
        int value = 0; glGetProgramiv(program, propertyName, &value); return value;
    }
    static int glShaderInt(GLuint shader, GLenum propertyName) {
        int value = 0; glGetShaderiv(shader, propertyName, &value); return value;
    }
    
    static void checkShaderLog(GLuint program)
    {
        GLboolean prog = glIsProgram(program);
        int logLength  = prog ? glProgramInt(program, GL_INFO_LOG_LENGTH) :
                                glShaderInt(program, GL_INFO_LOG_LENGTH);
        if (logLength > 1) {
            char* log = (char*)alloca(logLength);
            if (prog) glGetProgramInfoLog(program, logLength, &logLength, log);
            else      glGetShaderInfoLog(program,  logLength, &logLength, log);
            printf("%s\n", log); // this might just be a warning
        }
    }

    static GLuint compileShader(const char* sourceStr, int sourceLen, 
                                const string& sourceName, GLenum type)
    {
        GLuint shader = glCreateShader(type);
        if (!shader && glCreateShaderObjectARB) {
            shader = glCreateShaderObjectARB(type);
        }
        if (!shader) {
            LogError("glCreateShader failed: did you bind a valid GL 2.0+ context OR GL 1.0 context with GL_ARB_shader_object extension?");
            const bool shadersARB = glIsExtAvailable(glGetString(GL_EXTENSIONS), "GL_ARB_shader_object");
            LogError("glIsExtAvailable(\"GL_ARB_shader_object\"): %s", shadersARB ? "YES" : "NO");
            return 0;
        }

        const char* defines;
        #if __IPHONEOS__ || __EMSCRIPTEN__
            defines = "#version 100 // OpenGL ES 1.0\n";
        #else
            // get the actual GLSL version:
            float version = strview{ (char*)glGetString(GL_SHADING_LANGUAGE_VERSION) }.next_float();
            if (version <= 1.2f) // OpenGL 2.0 and 2.1
                defines = "#version 120 // OpenGL 2.1 \n"
                          "#define highp \n"
                          "#define lowp  \n";
            else if (1.3f <= version && version <= 1.5f) // GL 3.0, 3.1, 3.2
                defines = "#version 130 // OpenGL 3.0 \n"
                          "#define highp \n"
                          "#define lowp  \n";
            else if (3.3f <= version && version <= 4.2f) // GL 3.3, 4.0, 4.1, 4.2
                defines = "#version 330 // OpenGL 3.3 \n"
                          "#define highp \n"
                          "#define lowp  \n";
            else
                defines = "#version 120 // OpenGL 2.1 \n";

        #endif
        const int definesLen = (int)strlen(defines);

        // concatenate the shader for easier debugging in CodeXL
        int   shaderLen = definesLen + sourceLen;
        char* shaderStr = (char*)alloca(shaderLen + 1);
        memcpy(shaderStr, defines, (size_t)definesLen);
        memcpy(shaderStr + definesLen, sourceStr, (size_t)sourceLen);
        shaderStr[shaderLen] = '\0';

        glShaderSource(shader, 1, (const char**)&shaderStr, &shaderLen);
        glCompileShader(shader);
        checkShaderLog(shader); // this can be a warning
        int status = glShaderInt(shader, GL_COMPILE_STATUS);
        if (!status) {
            LogWarning("error: failed to compile '%s'", sourceName.c_str());
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }
    static GLuint compileShaderFile(const string& filename, time_t* modified, GLenum type)
    {
        auto f = rpp::file{filename, rpp::file::READONLY};
        if (!f) {
            //LogError("error: failed to open file '%s'", filename.c_str());
            return 0;
        }

        int size = f.size_and_time_modified(modified);

        char* mem = (char*)(size <= 65536 ? alloca(size) : malloc(size_t(size)));
        f.read(mem, size);
        f.close();

        GLuint shader = compileShader(mem, size, filename, type);
        if (size > 65536) free(mem);
        return shader;
    }

    ////////////////////////////////////////////////////////////////////////////////

    Shader::Shader() : program(0), vsMod(0), fsMod(0)
    {
        memset(uniforms, -1, sizeof(uniforms));
    }

    Shader::~Shader()
    {
        if (program) glDeleteProgram(program);
    }
    
    string Shader::name() const
    {
        return rpp::file_name(vsPath);
    }

    bool Shader::loadShader(const string& shaderName)
    {
        vsPath = shaderName + ".vert";
        fsPath = shaderName + ".frag";
        vsMod = 0;
        fsMod = 0;
        memset(uniforms,   -1,    sizeof(uniforms));
        memset(attributes, false, sizeof(attributes));
        return reload();
    }
    
    bool Shader::hotload()
    {
        if (rpp::file_modified(vsPath) != vsMod) return reload();
        if (rpp::file_modified(fsPath) != fsMod) return reload();
        return false;
    }

    bool Shader::reload()
    {
        //printf("loading shader %s|.frag\n", vs_path);
        GLuint vs = compileShaderFile(vsPath, &vsMod, GL_VERTEX_SHADER);
        GLuint fs = compileShaderFile(fsPath, &fsMod, GL_FRAGMENT_SHADER);

        // one of the files not found? then fall back to engine shader
        if (!vsMod || !fsMod) {
            if (ShaderPair s = GetEngineShaderSource(rpp::file_name(vsPath))) {
                if (vs) glDeleteShader(vs);
                if (fs) glDeleteShader(fs);
                vs = compileShader(s.vert.str, s.vert.len, vsPath, GL_VERTEX_SHADER);
                fs = compileShader(s.frag.str, s.frag.len, fsPath, GL_FRAGMENT_SHADER);
            }
            else LogWarning("error: failed to get fallback shader for '%s'", vsPath.c_str());
        }

        int status = vs && fs; // ok?
        if (status) {
            glDeleteProgram(program);
            GLuint sp = program = glCreateProgram();
            glAttachShader(sp, vs);
            glAttachShader(sp, fs);

            // bind our hard-coded attribute locations:
            for (GLuint i = 0; i < a_MaxAttributes; ++i)
                glBindAttribLocation(sp, i, AttributeMap[i]);

            glLinkProgram(sp);
            status = glProgramInt(sp, GL_LINK_STATUS);
            if (!status) {
                LogError("error: (%s|.frag) link failed!", vsPath.c_str());
                checkShaderLog(sp);
            }
            else {
                loadUniforms();

                // assign texture unit 0 to diffuseTex uniform:
                if (uniforms[u_DiffuseTex] != -1) {
                    glUseProgram(sp);
                    glUniform1i(uniforms[u_DiffuseTex], 0);
                    glUseProgram(0);
                }
                
                glValidateProgram(sp);
                status = glProgramInt(sp, GL_VALIDATE_STATUS);
                if (!status) LogError("error: (%s|.frag) validate failed!", vsPath.c_str());
                
                checkShaderLog(sp); // this can be a warning, so we always display log
            }
        }
        glDeleteShader(vs);
        glDeleteShader(fs);
        return status != 0;
    }
    
    void Shader::unload()
    {
        if (program)
        {
            glDeleteProgram(program);
            program = 0;
        }
    }

    void Shader::loadUniforms()
    {
        // brute force load all supported uniform and attribute locations
        int numActive;
        glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &numActive);
        for (int i = 0; numActive && i < u_MaxUniforms; ++i) {
            int loc = glGetUniformLocation(program, UniformMap[i]);
            if (loc != -1) {
                --numActive;
                //printf(" uniform %d %s\n", loc, UniformMap[i]);
            }
            uniforms[i] = (char)loc; // always write result (in case of shader reload)
        }
        //glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &numActive);
        for (int i = 0; i < a_MaxAttributes; ++i) {
            int loc = glGetAttribLocation(program, AttributeMap[i]);
            if (loc != -1) {
                //printf(" attrib  %d %s\n", loc, AttributeMap[i]);
            }
            attributes[i] = loc != -1; // always write result (in case of shader reload)
        }
    }

    static int getActiveProgram()
    {
        int program; glGetIntegerv(GL_CURRENT_PROGRAM, &program); return program;
    }

    void Shader::bind()
    {
        if (program != (uint32_t)getActiveProgram()) {
            glUseProgram(program);
        }
    }

    void Shader::unbind()
    {
        if (program == (uint32_t)getActiveProgram()) {
            glUseProgram(0);
        }
    }

    bool Shader::activeUniform(ShaderUniform uniformSlot) const
    {
        return uniforms[uniformSlot] != -1;
    }

    bool Shader::activeAttrib(ShaderAttr attrSlot) const
    {
        return attributes[attrSlot];
    }

    void Shader::checkUniform(const char* where, ShaderUniform uniformSlot) const
    {
        if (!getActiveProgram())
            LogError("%s: no active shader program", where);
        if (uniformSlot < 0 || u_MaxUniforms <= uniformSlot)
            LogError("%s: uniform %d is invalid", where, uniformSlot);
        if (uniforms[uniformSlot] == -1)
            LogError("%s: uniform '%s' not found", where, uniform_name(uniformSlot));
    }

    void Shader::bind(ShaderUniform uniformSlot, const Matrix4& matrix)
    {
        checkUniform("shader_bind_mat4()", uniformSlot);
        glUniformMatrix4fv(uniforms[uniformSlot], 1, GL_FALSE, matrix.m);
    }
    
    void Shader::bind(ShaderUniform uniformSlot, unsigned glTexture)
    {
        checkUniform("shader_bind_tex()", uniformSlot);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, glTexture);
        glUniform1i(uniforms[uniformSlot], 0); // 0=GL_TEXTURE0
    }
    
    void Shader::bind(ShaderUniform uniformSlot, const Texture& texture)
    {
        bind(uniformSlot, texture.nativeHandle());
    }
    
    void Shader::bind(ShaderUniform uniformSlot, const Texture* texture)
    {
        bind(uniformSlot, texture ? texture->nativeHandle() : 0u);
    }
    
    void Shader::bind(ShaderUniform uniformSlot, const Vector2& value)
    {
        checkUniform("shader_bind_vec2()", uniformSlot);
        glUniform2fv(uniforms[uniformSlot], 1, &value.x);
    }
    
    void Shader::bind(ShaderUniform uniformSlot, const Vector3& value)
    {
        checkUniform("shader_bind_vec3()", uniformSlot);
        glUniform3fv(uniforms[uniformSlot], 1, &value.x);
    }
    
    void Shader::bind(ShaderUniform uniformSlot, const Vector4& value)
    {
        checkUniform("shader_bind_vec4()", uniformSlot);
        glUniform4fv(uniforms[uniformSlot], 1, &value.x);
    }

    ////////////////////////////////////////////////////////////////////////////////
}

