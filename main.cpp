#include <GL/glut.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kGravity = 5.8f;
constexpr float kPoolRadius = 2.15f;
constexpr float kRimHeight = 0.22f;
constexpr int kInitialParticleCount = 1400;

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Particle {
    Vec3 position;
    Vec3 velocity;
    float age = 0.0f;
    float lifetime = 1.0f;
};

struct Ripple {
    float x = 0.0f;
    float z = 0.0f;
    float age = 0.0f;
    float lifetime = 0.9f;
};

std::vector<Particle> particles;
std::vector<Ripple> ripples;
int particleCount = kInitialParticleCount;
int windowWidth = 1024;
int windowHeight = 768;
int previousFrameMs = 0;
int previousMouseX = 0;
int previousMouseY = 0;
bool mouseDragging = false;
bool paused = false;
bool showHelp = true;
float cameraYaw = 35.0f;
float cameraPitch = 24.0f;
float cameraDistance = 9.0f;
float fountainHeight = 3.2f;
float emitterRate = 700.0f;
float emissionRemainder = 0.0f;
int colorMode = 0;
int sprayMode = 1;
int fpsFrames = 0;
int fpsWindowStartMs = 0;
int displayedFps = 0;

float randomRange(float minimum, float maximum) {
    const float unit = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
    return minimum + unit * (maximum - minimum);
}

void setParticleCount(int count) {
    particleCount = std::max(200, std::min(6000, count));
    particles.resize(static_cast<std::size_t>(particleCount));
    for (Particle& particle : particles) {
        particle.age = particle.lifetime;
    }
    emissionRemainder = 0.0f;
}

void respawn(Particle& particle) {
    const float angle = randomRange(0.0f, 2.0f * kPi);
    float minimumSpread = 0.25f;
    float maximumSpread = 1.35f;
    if (sprayMode == 0) {
        minimumSpread = 0.12f;
        maximumSpread = 0.72f;
    } else if (sprayMode == 2) {
        minimumSpread = 0.75f;
        maximumSpread = 2.15f;
    }
    const float horizontalSpeed = randomRange(minimumSpread, maximumSpread);
    particle.position = {0.0f, kRimHeight + 0.04f, 0.0f};
    const float rise = std::max(0.5f, fountainHeight - particle.position.y);
    const float verticalSpeed = std::sqrt(2.0f * kGravity * rise) * randomRange(0.97f, 1.03f);
    particle.velocity = {
        std::cos(angle) * horizontalSpeed,
        verticalSpeed,
        std::sin(angle) * horizontalSpeed,
    };
    particle.age = 0.0f;
    particle.lifetime = 2.0f * verticalSpeed / kGravity + 0.35f;
}

void resetSimulation() {
    for (Particle& particle : particles) {
        particle.age = particle.lifetime;
    }
    emissionRemainder = 0.0f;
    ripples.clear();
}

void updateSimulation(float deltaSeconds) {
    if (paused) return;

    for (Ripple& ripple : ripples) ripple.age += deltaSeconds;
    ripples.erase(std::remove_if(ripples.begin(), ripples.end(), [](const Ripple& ripple) {
        return ripple.age >= ripple.lifetime;
    }), ripples.end());

    emissionRemainder += emitterRate * deltaSeconds;
    int toEmit = static_cast<int>(emissionRemainder);
    emissionRemainder -= static_cast<float>(toEmit);

    for (Particle& particle : particles) {
        if (particle.age >= particle.lifetime && toEmit > 0) {
            respawn(particle);
            --toEmit;
        }
        if (particle.age >= particle.lifetime) continue;

        particle.age += deltaSeconds;
        particle.velocity.y -= kGravity * deltaSeconds;
        particle.position.x += particle.velocity.x * deltaSeconds;
        particle.position.y += particle.velocity.y * deltaSeconds;
        particle.position.z += particle.velocity.z * deltaSeconds;

        if (particle.position.y <= kRimHeight + 0.04f) {
            const float distanceFromCenter = std::sqrt(
                particle.position.x * particle.position.x + particle.position.z * particle.position.z);
            if (distanceFromCenter < kPoolRadius * 0.68f && ripples.size() < 120 &&
                randomRange(0.0f, 1.0f) < 0.16f) {
                ripples.push_back({particle.position.x, particle.position.z, 0.0f, randomRange(0.65f, 1.0f)});
            }
            particle.age = particle.lifetime;
        }
    }
}

void drawText(float x, float y, const std::string& text) {
    glRasterPos2f(x, y);
    for (unsigned char character : text) {
        glutBitmapCharacter(GLUT_BITMAP_8_BY_13, character);
    }
}

void drawHelp() {
    if (!showHelp) return;
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0.0, static_cast<double>(windowWidth), 0.0, static_cast<double>(windowHeight));
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.035f, 0.075f, 0.11f, 0.88f);
    glBegin(GL_QUADS);
    glVertex2f(16.0f, static_cast<float>(windowHeight) - 174.0f);
    glVertex2f(485.0f, static_cast<float>(windowHeight) - 174.0f);
    glVertex2f(485.0f, static_cast<float>(windowHeight) - 15.0f);
    glVertex2f(16.0f, static_cast<float>(windowHeight) - 15.0f);
    glEnd();
    glColor3f(0.77f, 0.92f, 1.0f);
    drawText(30.0f, static_cast<float>(windowHeight) - 38.0f, "FLOWING FOUNTAIN  |  OpenGL particle simulation");
    drawText(30.0f, static_cast<float>(windowHeight) - 60.0f, "Drag: orbit camera    Wheel: zoom    Space: pause/resume");
    drawText(30.0f, static_cast<float>(windowHeight) - 82.0f, "[ / ]: particles    - / +: height    C: color    M: spray style");
    drawText(30.0f, static_cast<float>(windowHeight) - 104.0f, "R: reset    H: hide help    Esc: quit");
    drawText(30.0f, static_cast<float>(windowHeight) - 128.0f,
             "Pattern: " + std::string(sprayMode == 0 ? "Focused" : sprayMode == 1 ? "Classic" : "Wide") +
                 "    FPS: " + std::to_string(displayedFps));
    drawText(30.0f, static_cast<float>(windowHeight) - 151.0f,
             "Particles: " + std::to_string(particleCount) + "    Height: " + std::to_string(fountainHeight).substr(0, 3) +
                 "    " + (paused ? "PAUSED" : "LIVE"));
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glDisable(GL_BLEND);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void drawPool() {
    const int segments = 96;
    const float outerRadius = kPoolRadius;
    const float innerRadius = kPoolRadius * 0.82f;

    glColor3f(0.13f, 0.25f, 0.31f);
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= segments; ++i) {
        const float angle = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(segments);
        const float x = std::cos(angle);
        const float z = std::sin(angle);
        glNormal3f(x, 0.0f, z);
        glVertex3f(x * outerRadius, 0.0f, z * outerRadius);
        glVertex3f(x * outerRadius, kRimHeight, z * outerRadius);
    }
    glEnd();

    glColor3f(0.56f, 0.69f, 0.68f);
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= segments; ++i) {
        const float angle = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(segments);
        const float x = std::cos(angle);
        const float z = std::sin(angle);
        glNormal3f(0.0f, 1.0f, 0.0f);
        glVertex3f(x * innerRadius, kRimHeight, z * innerRadius);
        glVertex3f(x * outerRadius, kRimHeight, z * outerRadius);
    }
    glEnd();

    glColor3f(0.025f, 0.20f, 0.29f);
    glBegin(GL_TRIANGLE_FAN);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(0.0f, kRimHeight + 0.015f, 0.0f);
    for (int i = 0; i <= segments; ++i) {
        const float angle = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(segments);
        glVertex3f(std::cos(angle) * innerRadius, kRimHeight + 0.015f,
                   std::sin(angle) * innerRadius);
    }
    glEnd();
}

void drawParticles() {
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPointSize(3.0f);
    if (colorMode == 0) glColor4f(0.35f, 0.78f, 1.0f, 0.82f);
    else if (colorMode == 1) glColor4f(0.55f, 1.0f, 0.82f, 0.82f);
    else glColor4f(0.76f, 0.65f, 1.0f, 0.82f);

    glBegin(GL_POINTS);
    for (const Particle& particle : particles) {
        if (particle.age < particle.lifetime) {
            glVertex3f(particle.position.x, particle.position.y, particle.position.z);
        }
    }
    glEnd();
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

void drawRipples() {
    if (ripples.empty()) return;
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(2.0f);
    for (const Ripple& ripple : ripples) {
        const float progress = ripple.age / ripple.lifetime;
        const float radius = 0.08f + progress * 0.95f;
        const float alpha = (1.0f - progress) * 0.72f;
        if (colorMode == 0) glColor4f(0.48f, 0.84f, 1.0f, alpha);
        else if (colorMode == 1) glColor4f(0.58f, 1.0f, 0.84f, alpha);
        else glColor4f(0.82f, 0.72f, 1.0f, alpha);

        glBegin(GL_LINE_LOOP);
        for (int segment = 0; segment < 32; ++segment) {
            const float angle = 2.0f * kPi * static_cast<float>(segment) / 32.0f;
            glVertex3f(ripple.x + std::cos(angle) * radius, kRimHeight + 0.04f,
                       ripple.z + std::sin(angle) * radius);
        }
        glEnd();
    }
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    const float yaw = cameraYaw * kPi / 180.0f;
    const float pitch = cameraPitch * kPi / 180.0f;
    const float horizontal = cameraDistance * std::cos(pitch);
    gluLookAt(horizontal * std::sin(yaw), cameraDistance * std::sin(pitch),
              horizontal * std::cos(yaw), 0.0, 1.25, 0.0, 0.0, 1.0, 0.0);

    const GLfloat lightPosition[] = {4.0f, 8.0f, 5.0f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
    drawPool();
    drawRipples();
    drawParticles();
    drawHelp();
    glutSwapBuffers();
}

void update(int) {
    const int now = glutGet(GLUT_ELAPSED_TIME);
    ++fpsFrames;
    if (now - fpsWindowStartMs >= 1000) {
        displayedFps = fpsFrames;
        fpsFrames = 0;
        fpsWindowStartMs = now;
    }
    float deltaSeconds = static_cast<float>(now - previousFrameMs) / 1000.0f;
    previousFrameMs = now;
    deltaSeconds = std::max(0.0f, std::min(deltaSeconds, 0.04f));
    updateSimulation(deltaSeconds);
    glutPostRedisplay();
    glutTimerFunc(16, update, 0);
}

void reshape(int width, int height) {
    windowWidth = std::max(width, 1);
    windowHeight = std::max(height, 1);
    glViewport(0, 0, windowWidth, windowHeight);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(48.0, static_cast<double>(windowWidth) / static_cast<double>(windowHeight), 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW);
}

void keyboard(unsigned char key, int, int) {
    switch (key) {
        case 27: std::exit(EXIT_SUCCESS);
        case ' ': paused = !paused; break;
        case 'r': case 'R': resetSimulation(); break;
        case 'h': case 'H': showHelp = !showHelp; break;
        case 'c': case 'C': colorMode = (colorMode + 1) % 3; break;
        case 'm': case 'M': sprayMode = (sprayMode + 1) % 3; break;
        case '[': setParticleCount(particleCount - 200); break;
        case ']': setParticleCount(particleCount + 200); break;
        case '-': case '_': fountainHeight = std::max(1.5f, fountainHeight - 0.2f); break;
        case '+': case '=': fountainHeight = std::min(5.0f, fountainHeight + 0.2f); break;
        default: return;
    }
    glutPostRedisplay();
}

void mouseButton(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON) {
        mouseDragging = (state == GLUT_DOWN);
        previousMouseX = x;
        previousMouseY = y;
    }
    if (button == 3 && state == GLUT_DOWN) cameraDistance = std::max(4.0f, cameraDistance - 0.5f);
    if (button == 4 && state == GLUT_DOWN) cameraDistance = std::min(18.0f, cameraDistance + 0.5f);
    glutPostRedisplay();
}

void mouseMotion(int x, int y) {
    if (!mouseDragging) return;
    cameraYaw += static_cast<float>(x - previousMouseX) * 0.35f;
    cameraPitch += static_cast<float>(y - previousMouseY) * 0.35f;
    cameraPitch = std::max(-5.0f, std::min(cameraPitch, 75.0f));
    previousMouseX = x;
    previousMouseY = y;
    glutPostRedisplay();
}

void initialize() {
    std::srand(20260926);
    setParticleCount(kInitialParticleCount);
    glClearColor(0.035f, 0.065f, 0.095f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_POINT_SMOOTH);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glShadeModel(GL_SMOOTH);
    glPointSize(3.0f);
}
}  // namespace

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(windowWidth, windowHeight);
    glutCreateWindow("Flowing Fountain | Interactive OpenGL Simulation");
    initialize();
    previousFrameMs = glutGet(GLUT_ELAPSED_TIME);
    fpsWindowStartMs = previousFrameMs;

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutMouseFunc(mouseButton);
    glutMotionFunc(mouseMotion);
    glutTimerFunc(16, update, 0);
    glutMainLoop();
    return EXIT_SUCCESS;
}
