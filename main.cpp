#include "main.hpp"

const int k_maxMessageLength = 1024;

volatile static int g_windowWidth = 640;
volatile static int g_windowHeight = 480;

static float cubicInterpolate(float t, float y0, float y1, float y2, float y3)
{
    return (
        (-y0 + 3 * y1 - 3 * y2 + y3) * t * t * t
        + (2 * y0 - 5 * y1 + 4 * y2 - y3) * t * t
        + (-y0 + y2) * t
        + 2 * y1
    ) * 0.5;
}

GLFWwindow* setUpWindowAndOpenGL(const char* windowTitle) {
    if (!glfwInit()) {
        throw std::runtime_error("GLFW initialization failed.");
    }
    GLFWwindow* window = glfwCreateWindow(g_windowWidth, g_windowHeight, windowTitle, NULL, NULL);
    if (!window) {
        glfwTerminate();
        throw std::runtime_error("GLFW initialization failed.");
    }
    glfwMakeContextCurrent(window);

    glewExperimental = GL_TRUE;
    glewInit();
    if (!glCreateShader) {
        throw std::runtime_error("Unsuccessful GLEW initialization.");
    }

    return window;
}

class MinimalOpenGLApp {
public:
    MinimalOpenGLApp(GLFWwindow* window)
    {
        m_window = window;
        glfwSetFramebufferSizeCallback(m_window, resize);
    }

    void setProgram(GLuint program)
    {
        m_program = program;
    }

    void setNumTriangles(int numTriangles)
    {
        m_numTriangles = numTriangles;
    }

private:
    const char* m_windowTitle;
    int m_numTriangles;
    GLFWwindow* m_window;
    GLuint m_program;

    void render()
    {
    }

    static void resize(GLFWwindow* window, int width, int height)
    {
        glViewport(0, 0, (GLsizei)width, (GLsizei)height);
        g_windowWidth = width;
        g_windowHeight = height;
    }
};

class ShaderProgram {
public:
    ShaderProgram(const char* vertexShaderSource, const char* fragmentShaderSource)
        : m_vertexShaderSource(vertexShaderSource)
        , m_fragmentShaderSource(fragmentShaderSource)
    {
        makeVertexShader();
        makeFragmentShader();
        makeProgram();
    }

    ~ShaderProgram()
    {
        cleanUp();
    }

    GLuint getProgram()
    {
        return m_program;
    }

    GLuint getAttribLocation(const char* key)
    {
        return glGetAttribLocation(m_program, key);
    }

private:
    const char* m_vertexShaderSource;
    const char* m_fragmentShaderSource;
    GLuint m_vertexShader;
    GLuint m_fragmentShader;
    GLuint m_program;

    void makeVertexShader()
    {
        m_vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(m_vertexShader, 1, &m_vertexShaderSource, NULL);
        glCompileShader(m_vertexShader);

        GLint success;
        glGetShaderiv(m_vertexShader, GL_COMPILE_STATUS, &success);
        if (success != GL_TRUE) {
            GLsizei logLength = 0;
            GLchar c_message[k_maxMessageLength];
            glGetShaderInfoLog(m_vertexShader, k_maxMessageLength, &logLength, c_message);
            std::string message = c_message;
            std::string full_message = "Error compiling vertex shader: " + message;
            throw std::runtime_error(full_message);
        }
    }

    void makeFragmentShader()
    {
        m_fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(m_fragmentShader, 1, &m_fragmentShaderSource, NULL);
        glCompileShader(m_fragmentShader);

        GLint success;
        glGetShaderiv(m_fragmentShader, GL_COMPILE_STATUS, &success);
        if (success != GL_TRUE) {
            GLsizei logLength = 0;
            GLchar c_message[k_maxMessageLength];
            glGetShaderInfoLog(m_fragmentShader, k_maxMessageLength, &logLength, c_message);
            std::string message = c_message;
            std::string full_message = "Error compiling fragment shader: " + message;
            throw std::runtime_error(full_message);
        }
    }

    void makeProgram()
    {
        m_program = glCreateProgram();
        glAttachShader(m_program, m_vertexShader);
        glAttachShader(m_program, m_fragmentShader);
        glLinkProgram(m_program);
    }

    void cleanUp()
    {
        glDetachShader(m_program, m_vertexShader);
        glDetachShader(m_program, m_fragmentShader);
        glDeleteProgram(m_program);
        glDeleteShader(m_vertexShader);
        glDeleteShader(m_fragmentShader);
    }
};

class Scope {
public:
    Scope(int numPoints)
    {
        const char* vertexShaderSource = ("#version 130\n"
                                          "in vec2 pos;\n"
                                          "void main()\n"
                                          "{\n"
                                          "    gl_Position = vec4(pos, 1, 1);\n"
                                          "}\n");

        const char* fragmentShaderSource = (
            "#version 130\n"
            "uniform vec2 windowSize;\n"
            "out vec3 fragColor;\n"
            "void main()\n"
            "{\n"
            "fragColor = vec3(1.0);\n"
            "}\n"
        );

        ShaderProgram shaderProgram(vertexShaderSource, fragmentShaderSource);
        m_program = shaderProgram.getProgram();

        m_numSegments = numPoints - 1;

        // Each segment has two points, plus an additional two points at the end
        // of the strip. Each point has two coordinates.
        m_coordinatesLength = 4 * (m_numSegments + 1);
        m_coordinates = new GLfloat[m_coordinatesLength];
        for (int i = 0; i < m_numSegments + 1; i++) {
            m_coordinates[4 * i + 0] = 0;
            m_coordinates[4 * i + 1] = -1;
            m_coordinates[4 * i + 2] = 0;
            m_coordinates[4 * i + 3] = 1;
        }

        m_numTriangles = 2 * m_numSegments;
        m_elementsLength = 3 * m_numTriangles;
        m_elements = new GLuint[m_elementsLength];
        for (int i = 0; i < m_numSegments; i++) {
            m_elements[6 * i + 0] = 2 * i + 0;
            m_elements[6 * i + 1] = 2 * i + 1;
            m_elements[6 * i + 2] = 2 * i + 2;
            m_elements[6 * i + 3] = 2 * i + 1;
            m_elements[6 * i + 4] = 2 * i + 2;
            m_elements[6 * i + 5] = 2 * i + 3;
        }

        makeVertexBuffer();
        makeArrayBuffer();
        makeElementBuffer();
    }

    ~Scope()
    {
        cleanUp();
    }

    int getProgram()
    {
        return m_program;
    }

    int getNumTriangles()
    {
        return m_numTriangles;
    }

    void setPlotX(std::vector<float>& plotX)
    {
        for (int i = 0; i < plotX.size(); i++) {
            m_coordinates[4 * i + 0] = 2 * plotX[i] - 1;
            m_coordinates[4 * i + 2] = 2 * plotX[i] - 1;
        }
    }

    void setPlotY(std::vector<float>& plotY)
    {
        float thicknessInWindowCoordinates = m_thicknessInPixels / g_windowHeight;
        for (int i = 0; i < plotY.size(); i++) {
            m_coordinates[4 * i + 1] = 2 * plotY[i] - 1 + thicknessInWindowCoordinates;
            m_coordinates[4 * i + 3] = 2 * plotY[i] - 1 - thicknessInWindowCoordinates;
        }
    }

    void render()
    {
        glBufferData(GL_ARRAY_BUFFER, m_coordinatesLength * sizeof(GLfloat), m_coordinates, GL_STREAM_DRAW);

        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(m_program);

        GLuint color = glGetUniformLocation(m_program, "windowSize");
        glUniform2f(color, g_windowWidth, g_windowHeight);

        glDrawElements(GL_TRIANGLES, 3 * m_numTriangles, GL_UNSIGNED_INT, (void*)0);
    }

private:
    int m_numSegments = 64;
    int m_numTriangles;
    GLuint m_program;
    GLuint m_vao;
    GLuint m_vbo;
    GLuint m_ebo;
    GLfloat* m_coordinates;
    int m_coordinatesLength;
    GLuint* m_elements;
    int m_elementsLength;
    float m_thicknessInPixels = 10;

    void makeVertexBuffer()
    {
        glGenBuffers(1, &m_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    }

    void makeArrayBuffer()
    {
        glGenVertexArrays(1, &m_vao);
        glBindVertexArray(m_vao);
        glVertexAttribPointer(glGetAttribLocation(m_program, "pos"), 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
    }

    void makeElementBuffer()
    {
        glGenBuffers(1, &m_ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_elementsLength * sizeof(GLuint), m_elements, GL_STATIC_DRAW);
    }

    void cleanUp()
    {
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(0);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glDeleteBuffers(1, &m_ebo);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glDeleteBuffers(1, &m_vbo);

        glBindVertexArray(0);
        glDeleteVertexArrays(1, &m_vao);
    }
};

VisualizerAudioCallback::VisualizerAudioCallback(int bufferSize)
    : m_bufferSize(bufferSize)
{
    m_writePos = 0;
    m_buffer = static_cast<float*>(malloc(sizeof(float) * m_bufferSize));
    for (int i = 0; i < m_bufferSize; i++) {
        m_buffer[i] = 0;
    }
}

VisualizerAudioCallback::~VisualizerAudioCallback()
{
    free(m_buffer);
}

void VisualizerAudioCallback::process(InputBuffer input_buffer, OutputBuffer output_buffer, int frame_count)
{
    for (int i = 0; i < frame_count; i++) {
        m_buffer[m_writePos] = input_buffer[0][i];
        m_writePos += 1;
        if (m_writePos == m_bufferSize) {
            m_writePos = 0;
        }
    }
}

FFTAudioCallback::FFTAudioCallback(int bufferSize)
    : m_bufferSize(bufferSize),
    m_spectrumSize(bufferSize / 2 + 1),
    m_numChunks(0),
    m_numPlotPoints(0)
{
    m_writePos = 0;

    m_samples = static_cast<double*>(fftw_malloc(sizeof(double) * m_bufferSize));
    for (int i = 0; i < m_bufferSize; i++) {
        m_samples[i] = 0;
    }

    m_spectrum = static_cast<fftw_complex*>(
        fftw_malloc(sizeof(fftw_complex) * m_spectrumSize)
    );

    m_chunkX.reserve(m_spectrumSize);
    m_chunkY.reserve(m_spectrumSize);
    m_magnitudeSpectrum.resize(m_spectrumSize);

    m_plotX.reserve(m_spectrumSize * m_cubicResolution);
    m_plotY.reserve(m_spectrumSize * m_cubicResolution);

    m_binToChunk.reserve(m_spectrumSize);

    m_fftwPlan = fftw_plan_dft_r2c_1d(m_bufferSize, m_samples, m_spectrum, FFTW_MEASURE);
}

FFTAudioCallback::~FFTAudioCallback()
{
    fftw_destroy_plan(m_fftwPlan);
    fftw_free(m_samples);
    fftw_free(m_spectrum);
}

float FFTAudioCallback::fftBinToFrequency(int fftBin)
{
    return 24000 * static_cast<float>(fftBin) / m_spectrumSize;
}

static float erbs(float frequency) {
    return 21.4 * std::log10(0.00437f * frequency + 1);
}

float FFTAudioCallback::position(float frequency)
{
    return (erbs(frequency) - erbs(20)) / (erbs(20e3) - erbs(20));
}

void FFTAudioCallback::setWindowSize(int windowWidth, int windowHeight)
{
    int chunkN = 2;

    m_chunkX.clear();

    m_binToChunk.clear();
    m_binToChunk.resize(m_spectrumSize);

    bool foundMultiChunk = false;
    int firstMultiChunk;
    int firstMultiChunkOffset;
    float firstMultiChunkPosition;
    int chunkIndex;

    int lastNominalChunk = -1;

    for (int i = 0; i < m_spectrumSize; i++) {
        float frequency = fftBinToFrequency(i);
        float thePosition = position(frequency);
        if (thePosition > 1.0) {
            m_binToChunk[i] = -1;
            continue;
        }
        int nominalChunk = static_cast<int>(std::floor(thePosition * windowWidth / chunkN));
        if (foundMultiChunk) {
            chunkIndex = firstMultiChunk + nominalChunk - firstMultiChunkOffset;
            m_binToChunk[i] = chunkIndex;
            if (nominalChunk != lastNominalChunk) {
                m_chunkX.push_back(thePosition);
            }
        } else {
            chunkIndex = i;
            m_binToChunk[i] = i;
            // FIXME may be an off-by-one error in here for the first multichunk
            m_chunkX.push_back(thePosition);
            if (nominalChunk == lastNominalChunk) {
                std::cout << "Found multichunk, bin " << i << std::endl;
                foundMultiChunk = true;
                firstMultiChunk = i;
                firstMultiChunkOffset = nominalChunk;
                firstMultiChunkPosition = thePosition;
            }
        }
        lastNominalChunk = nominalChunk;
    }

    m_numChunks = m_chunkX.size();
    m_numPlotPoints = m_numChunks * m_cubicResolution;

    m_plotX.clear();
    m_plotX.resize(m_numPlotPoints);

    for (int i = 0; i < m_numPlotPoints; i++) {
        int t1 = i / m_cubicResolution;
        int t0 = std::max(t1 - 1, 0);
        int t2 = std::min(t1 + 1, m_numChunks - 1);
        int t3 = std::min(t1 + 2, m_numChunks - 1);
        float x0 = m_chunkX[t0];
        float x1 = m_chunkX[t1];
        float x2 = m_chunkX[t2];
        float x3 = m_chunkX[t3];
        float t = static_cast<float>(i) / m_cubicResolution - t1;
        m_plotX[i] = cubicInterpolate(t, x0, x1, x2, x3);
    }
}

void FFTAudioCallback::doFFT()
{
    if (m_numPlotPoints == 0) {
        return;
    }

    fftw_execute(m_fftwPlan);

    float maxDb;
    for (int i = 0; i < m_spectrumSize; i++) {
        float real = m_spectrum[i][0];
        float imag = m_spectrum[i][1];
        float magnitude = std::hypot(real, imag);
        float db = 20 * std::log10(magnitude);
        if (db > maxDb) {
            maxDb = db;
        }
        m_magnitudeSpectrum[i] = db;
    }

    if (maxDb > m_maxDb) {
        m_maxDb = maxDb;
    }

    m_chunkY.clear();
    m_chunkY.resize(m_numChunks);

    for (int i = 0; i < m_spectrumSize; i++) {
        m_magnitudeSpectrum[i] = (m_magnitudeSpectrum[i] - m_maxDb) / 60 + 1;
    }

    for (int i = 0; i < m_spectrumSize; i++) {
        int chunk = m_binToChunk[i];
        if (chunk < 0 || chunk >= m_chunkY.size()) {
            break;
        }
        m_chunkY[chunk] = std::max(m_chunkY[chunk], m_magnitudeSpectrum[i]);
    }

    m_plotY.clear();
    m_plotY.resize(m_numPlotPoints);
    for (int i = 0; i < m_numPlotPoints; i++) {
        int t1 = i / m_cubicResolution;
        int t0 = std::max(t1 - 1, 0);
        int t2 = std::min(t1 + 1, m_numChunks - 1);
        int t3 = std::min(t1 + 2, m_numChunks - 1);
        float y0 = m_chunkY[t0];
        float y1 = m_chunkY[t1];
        float y2 = m_chunkY[t2];
        float y3 = m_chunkY[t3];
        float t = static_cast<float>(i) / m_cubicResolution - t1;
        m_plotY[i] = cubicInterpolate(t, y0, y1, y2, y3);
    }
}

void FFTAudioCallback::process(InputBuffer input_buffer, OutputBuffer output_buffer, int frame_count)
{
    for (int i = 0; i < frame_count; i++) {
        m_samples[m_writePos] = input_buffer[0][i];
        m_writePos += 1;
        if (m_writePos == m_bufferSize) {
            doFFT();
            m_writePos = 0;
        }
    }
}


int main(int argc, char** argv)
{
    auto window = setUpWindowAndOpenGL("Scope");
    MinimalOpenGLApp app(window);

    FFTAudioCallback callback(1024);

    PortAudioBackend audioBackend(&callback);
    audioBackend.run();

    callback.setWindowSize(g_windowWidth, g_windowHeight);
    Scope scope(callback.getNumPlotPoints());
    scope.setPlotX(callback.getPlotX());

    while (!glfwWindowShouldClose(window)) {
        scope.setPlotY(callback.getPlotY());
        scope.render();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
