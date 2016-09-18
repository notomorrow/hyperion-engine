#include "particle_generator.h"
#include "../opengl.h"
#include "../rendering/shader_manager.h"
#include "../math/matrix_util.h"
#include <array>
#include <ctime>
#include <cassert>

namespace apex {
ParticleGenerator::ParticleGenerator(const ParticleConstructionInfo &info)
    : m_info(info),
      m_is_created(false),
      m_is_uploaded(false)
{
    // load shader
    m_shader = ShaderManager::GetInstance()->GetShader<ParticleShader>(ShaderProperties());

    // add some random particles
    srand(time(nullptr));
    for (int i = 0; i < m_info.m_max_particles; i++) {
        Particle particle;
        ResetParticle(particle);
        m_particles.push_back(particle);
    }
}

ParticleGenerator::~ParticleGenerator()
{
    if (m_is_created) {
        std::array<unsigned int, 4> buffers {
            m_vertex_buffer,
            m_position_buffer,
            m_lifespan_buffer,
            m_lookat_buffer
        };
        glDeleteBuffers(buffers.size(), &buffers[0]);
    }
}

void ParticleGenerator::ResetParticle(Particle &particle)
{
    double lifespan_random = MathUtil::EPSILON + MathUtil::Random(0.0, fabs(m_info.m_lifespan_randomness));

    particle.m_position = m_info.m_origin_generator(particle);
    particle.m_velocity = m_info.m_velocity_generator(particle);
    particle.m_life = 0.0;
    particle.m_lifespan = m_info.m_lifespan + lifespan_random;
    particle.m_alive = true;
}

Particle *ParticleGenerator::FindUnusedParticle()
{
    for (Particle &particle : m_particles) {
        if (!particle.m_alive) {
            return &particle;
        }
    }
    return nullptr;
}

void ParticleGenerator::UpdateParticles(double dt)
{
    for (Particle &particle : m_particles) {
        if (particle.m_alive) {
            particle.m_life += dt;
            particle.m_position += particle.m_velocity * dt;
        } else {
            // reset the particle if it is passed it's lifespan.
            ResetParticle(particle);
        }

        if (particle.m_life >= particle.m_lifespan) {
            // particle has passed it's lifespan, no longer alive
            particle.m_alive = false;
        }
    }
}

void ParticleGenerator::DrawParticles(Camera *camera)
{
    // vertices of a particle
    static const std::array<float, 12> vertices {
        -0.5f, -0.5f, 0.0f,
        0.5f, -0.5f, 0.0f,
        -0.5f, 0.5f, 0.0f,
        0.5f, 0.5f, 0.0f,
    };

    if (!m_is_created) {
        glGenBuffers(1, &m_vertex_buffer);
        glBindBuffer(GL_ARRAY_BUFFER, m_vertex_buffer);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);

        glGenBuffers(1, &m_position_buffer);
        glBindBuffer(GL_ARRAY_BUFFER, m_position_buffer);
        glBufferData(GL_ARRAY_BUFFER, m_info.m_max_particles * 3 * sizeof(float), nullptr, GL_STREAM_DRAW);

        glGenBuffers(1, &m_lifespan_buffer);
        glBindBuffer(GL_ARRAY_BUFFER, m_lifespan_buffer);
        glBufferData(GL_ARRAY_BUFFER, m_info.m_max_particles * sizeof(float), nullptr, GL_STREAM_DRAW);

        /*glGenBuffers(1, &m_lookat_buffer);
        glBindBuffer(GL_ARRAY_BUFFER, m_lookat_buffer);
        glBufferData(GL_ARRAY_BUFFER, m_info.m_max_particles * 16 * sizeof(float), nullptr, GL_STREAM_DRAW);*/

        m_is_created = true;
    }

    assert(m_particles.size() <= m_info.m_max_particles);

    // create and fill position buffer
    std::vector<float> positions(m_particles.size() * 3);
    {
        size_t counter = 0;
        for (Particle &particle : m_particles) {
            positions[counter] = particle.m_position.GetX();
            positions[counter + 1] = particle.m_position.GetY();
            positions[counter + 2] = particle.m_position.GetZ();
            counter += 3;
        }
    }

    // create and fill lifespan buffer
    std::vector<float> lifespans(m_particles.size());
    {
        size_t counter = 0;
        for (Particle &particle : m_particles) {
            double value;
            if (particle.m_life > particle.m_lifespan * 0.5) {
                value = 1.0 - (particle.m_life / particle.m_lifespan);
            } else {
                value = particle.m_life / particle.m_lifespan;
            }
            lifespans[counter] = (float)value;
            counter++;
        }
    }
    
    // create and fill lookat buffer
    // holds the all matrices from particle to eye position
    /*std::vector<float> lookat_matrices(m_particles.size() * 16);
    {
        size_t counter = 0;
        for (Particle &particle : m_particles) {
            Matrix4 lookat;
            MatrixUtil::ToLookAt(lookat, camera->GetTranslation() - particle.m_position, camera->GetUpVector());
            lookat.Transpose();
            for (float f : lookat.values) {
                lookat_matrices[counter++] = f;
            }
        }
    }*/

    m_shader->ApplyTransforms(Matrix4::Identity(), camera);
    m_shader->Use();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // upload position buffer
    glBindBuffer(GL_ARRAY_BUFFER, m_position_buffer);
    glBufferData(GL_ARRAY_BUFFER, m_info.m_max_particles * 3 * sizeof(float), nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, positions.size() * sizeof(float), &positions[0]);

    // upload lifespan buffer
    glBindBuffer(GL_ARRAY_BUFFER, m_lifespan_buffer);
    glBufferData(GL_ARRAY_BUFFER, m_info.m_max_particles * sizeof(float), nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, lifespans.size() * sizeof(float), &lifespans[0]);

    // upload lookat buffer
    /*glBindBuffer(GL_ARRAY_BUFFER, m_lookat_buffer);
    glBufferData(GL_ARRAY_BUFFER, m_info.m_max_particles * 16 * sizeof(float), nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, lookat_matrices.size() * sizeof(float), &lookat_matrices[0]);*/

    // draw the instanced particles
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, m_vertex_buffer);
    glVertexAttribPointer(
        0, // attribute. No particular reason for 0, but must match the layout in the shader.
        3, // size
        GL_FLOAT, // type
        false, // normalized?
        0, // stride
        (void*)0 // array buffer offset
        );

    // 2nd attribute buffer : positions of particles' centers
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, m_position_buffer);
    glVertexAttribPointer(
        1, // attribute. No particular reason for 1, but must match the layout in the shader.
        3, // size : x + y + z => 3
        GL_FLOAT, // type
        false, // normalized?
        0, // stride
        (void*)0 // array buffer offset
        );

    // 3rd attribute buffer : lifespans of particles
    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, m_lifespan_buffer);
    glVertexAttribPointer(
        2, // attribute. No particular reason for 1, but must match the layout in the shader.
        1, // size : x + y + z => 3
        GL_FLOAT, // type
        false, // normalized?
        0, // stride
        (void*)0 // array buffer offset
        );

    // upload lookat matrices
    /*glEnableVertexAttribArray(3);
    glEnableVertexAttribArray(4);
    glEnableVertexAttribArray(5);
    glEnableVertexAttribArray(6);
    glBindBuffer(GL_ARRAY_BUFFER, m_lookat_buffer);
    glVertexAttribPointer(3, 4, GL_FLOAT, false, sizeof(float) * 16, (void*)0);
    glVertexAttribPointer(4, 4, GL_FLOAT, false, sizeof(float) * 16, (void*)(sizeof(float) * 4));
    glVertexAttribPointer(5, 4, GL_FLOAT, false, sizeof(float) * 16, (void*)(sizeof(float) * 8));
    glVertexAttribPointer(6, 4, GL_FLOAT, false, sizeof(float) * 16, (void*)(sizeof(float) * 12));*/

    glVertexAttribDivisor(0, 0);
    glVertexAttribDivisor(1, 1);
    glVertexAttribDivisor(2, 1);

    // draw particles
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, m_particles.size());

    // reset changes
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glVertexAttribDivisor(0, 0);
    glVertexAttribDivisor(1, 0);
    glVertexAttribDivisor(2, 0);

    glDisable(GL_BLEND);

    m_shader->End();
}
} // namespace apex