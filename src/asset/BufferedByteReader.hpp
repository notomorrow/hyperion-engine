#ifndef BUFFERED_BYTE_READER_H
#define BUFFERED_BYTE_READER_H

#include <math/MathUtil.hpp>

#include <Types.hpp>

#include <Util.hpp>
#include <util/Defines.hpp>
#include <util/StringUtil.hpp>
#include <core/Core.hpp>

#include <array>
#include <fstream>
#include <type_traits>
#include <thread>
#include <mutex>

namespace hyperion {
template <SizeType BufferSize, class Byte = UByte>
class BufferedReader
{
public:
    static_assert(sizeof(Byte) == 1);

    BufferedReader(const std::string &filepath, std::streampos begin = 0)
        : filepath(filepath),
          file(nullptr)
    {
        file = new std::ifstream(filepath, std::ifstream::in | std::ifstream::ate | std::ifstream::binary);

        max_pos = file->tellg();
        file->seekg(begin);
        pos = file->tellg();
    }

    BufferedReader(const BufferedReader &other) = delete;
    BufferedReader &operator=(const BufferedReader &other) = delete;

    BufferedReader(BufferedReader &&other) noexcept
        : filepath(std::move(other.filepath)),
          max_pos(std::move(other.max_pos)),
          pos(std::move(other.pos)),
          file(std::move(other.file)),
          buffer(std::move(other.buffer))
    {
        other.file = nullptr;
    }

    BufferedReader &operator=(BufferedReader &&other) noexcept
    {
        if (&other == this) {
            return *this;
        }

        Close();

        if (file != nullptr) {
            delete file;
        }

        filepath = std::move(other.filepath);
        max_pos = std::move(other.max_pos),
        pos = std::move(other.max_pos);
        file = std::move(other.file);
        buffer = std::move(other.buffer);

        other.file    = nullptr;
        other.pos     = 0;
        other.max_pos = 0;

        return *this;
    }

    ~BufferedReader()
    {
        Close();

        delete file;
    }

    /*! \brief Returns a boolean indicating whether or not the file could be opened without issue */
    explicit operator bool() const
        { return IsOpen(); }

    const std::string &GetFilepath() const
        { return filepath; }

    /*! \brief Returns a boolean indicating whether or not the file could be opened without issue */
    bool IsOpen() const
        { return file->good(); }

    SizeType Position() const
        { return pos; }

    SizeType Max() const
        { return max_pos; }

    bool Eof() const
        { return pos >= max_pos; }

    void Rewind(unsigned long amount)
    {
        if (amount > pos) {
            pos = 0;
        } else {
            pos -= amount;
        }

        if (!Eof()) {
            file->seekg(pos -= amount);
        }
    }

    void Skip(unsigned long amount)
    {
        pos += amount;

        if (!Eof()) {
            file->seekg(pos);
        }
    }

    void Seek(unsigned long where_to)
    {
        pos = where_to;

        if (!Eof()) {
            file->seekg(pos);
        }
    }

    void Close()
    {
        if (file == nullptr) {
            return;
        }

        file->close();

        pos = max_pos; // eof
    }

    /*! \brief Reads the entirety of the remaining bytes in file and returns a vector of
     * unsigned bytes. Note that using this method to read the whole file in one call bypasses
     * the intention of having a buffered reader. */
    std::vector<Byte> ReadBytes()
    {
        if (Eof()) {
            return {};
        }

        const SizeType remaining = max_pos - pos;
        
        std::vector<Byte> bytes;
        bytes.resize(remaining);

        file->read(reinterpret_cast<char *>(&bytes[0]), bytes.size());
        pos += bytes.size();

        return bytes;
    }

    /*! \brief Reads the entirety of the remaining bytes in file PER LINE and returns a vector of
     * string. Note that using this method to read the whole file in one call bypasses
     * the intention of having a buffered reader. */
    std::vector<std::string> ReadAllLines()
    {
        if (Eof()) {
            return {};
        }
        
        std::vector<std::string> lines;

        ReadLines([&lines](const std::string &line) {
            lines.push_back(line);
        });

        return lines;
    }

    SizeType Read(void *ptr, SizeType count)
    {
        return Read(ptr, count, [](void *ptr, const Byte *buffer, SizeType chunk_size) {
           Memory::Copy(ptr, buffer, chunk_size);
        });
    }

    /*! @returns The total number of bytes read */
    template <class Lambda = std::add_pointer_t<void(void *, const Byte *, SizeType)>>
    SizeType Read(void *ptr, SizeType count, Lambda &&func)
    {
        if (Eof()) {
            return 0;
        }

        SizeType total_read = 0;

        while (count) {
            const SizeType chunk_requested = MathUtil::Min(count, BufferSize);
            const SizeType chunk_returned = Read(chunk_requested);
            const SizeType offset = total_read;

            func(reinterpret_cast<void *>(uintptr_t(ptr) + offset), &buffer[0], chunk_returned);

            total_read += chunk_returned;

            if (chunk_returned < chunk_requested) {
                /* File ended */
                break;
            }

            count -= chunk_returned;
        }

        return total_read;
    }

    /*! \brief Read one instance of the given template type, using sizeof(T).
     * @param ptr The pointer to T, where the read memory will be written.
     * @returns The number of bytes read
     */
    template <class T>
    SizeType Read(T *ptr)
    {
        return Read(static_cast<void *>(ptr), sizeof(T));
    }

    template <class Functor, class ...Args>
    auto ReadLinesParallel(int num_threads, Args &&... args)
    {
        constexpr int use_threading_threshold = 8; // num lines has to be x*num_threads to qualify

        std::vector<std::string> all_lines;

        ReadLines([&all_lines](const std::string &line) {
            all_lines.push_back(line);
        }, false);

        if (num_threads * use_threading_threshold > all_lines.size()) {
            num_threads = 1; // do not use threading
        }

        const SizeType num_lines_per_thread = all_lines.size() / num_threads;

        SizeType lines_remaining = all_lines.size();
        SizeType offset = 0;

        std::vector<std::thread> threads;
        std::mutex mutex;

        std::vector<Functor> functors(num_threads);

        for (int i = 0; i < num_threads; i++) {
            const SizeType num_lines_this_thread = i == num_threads - 1
                ? lines_remaining
                : num_lines_per_thread;

            threads.emplace_back([i, offset, num_lines_this_thread, &all_lines, &functors, &mutex, &args...]() mutable { // perfect capture, c++20 feature
                functors[i] = Functor{};

                for (SizeType j = offset; j < offset + num_lines_this_thread; j++) {
                    functors[i].HandleLine(all_lines[j], mutex, args...);
                }
            });
            
            offset += num_lines_this_thread;
            lines_remaining -= num_lines_this_thread;
        }

        for (auto &th : threads) {
            th.join();
        }

        return functors;
    }

    template <class LambdaFunction>
    void ReadLines(LambdaFunction func, bool buffered = true)
    {
        if (!buffered) {
            auto all_bytes = ReadBytes();

            std::string accum;
            accum.reserve(BufferSize);
            
            for (SizeType i = 0; i < all_bytes.size(); i++) {
                if (all_bytes[i] == '\n') {
                    func(accum);
                    accum.clear();
                    
                    continue;
                }

                accum += all_bytes[i];
            }

            if (accum.length() != 0) {
                func(accum);
            }

            return;
        }

        // if there is no newline in buffer:
        // accumulate chars until a newline is found, then
        // call func with the accumulated line
        // if there is at least one newline in buffer:
        // call func for each line in the buffer,
        // holding onto the last one (assuming no newline at the end)
        // keep that last line and continue accumulating until a newline is found
        std::string accum;
        accum.reserve(BufferSize);

        while (SizeType count = Read()) {
            for (SizeType i = 0; i < count; i++) {
                if (buffer[i] == '\n') {
                    func(accum);
                    accum.clear();
                    
                    continue;
                }

                accum += buffer[i];
            }
        }

        if (accum.length() != 0) {
            func(accum);
        }
    }

    template <class LambdaFunction>
    void ReadChars(LambdaFunction func)
    {
        while (SizeType count = Read()) {
            for (SizeType i = 0; i < count; i++) {
                func(static_cast<char>(buffer[i]));
            }
        }
    }

private:
    std::string filepath;
    std::ifstream *file;
    std::streampos pos;
    std::streampos max_pos;
    std::array<Byte, BufferSize> buffer{};

    SizeType Read()
    {
        if (Eof()) {
            return 0;
        }

        file->read(reinterpret_cast<char *>(&buffer[0]), BufferSize);

        const SizeType count = file->gcount();

        AssertThrow(count <= BufferSize);

        pos += count;

        return count;
    }

    SizeType Read(SizeType sz)
    {
        AssertThrow(sz <= BufferSize);

        if (Eof()) {
            return 0;
        }

        file->read(reinterpret_cast<char *>(&buffer[0]), sz);

        const SizeType count = file->gcount();
        pos += count;

        return count;
    }

    void ReadBytes(Byte *ptr, unsigned size)
    {
        file->read(reinterpret_cast<char *>(ptr), size);
        pos += size;
    }
};

using Reader = BufferedReader<2048>;

} // namespace hyperion

#endif
