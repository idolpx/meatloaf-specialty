
#include "lbr.h"

#include "endianness.h"

/********************************************************
 * Streams
 ********************************************************/

int8_t LBRMStream::loadEntries()
{
    std::string signature = readUntil(0x20);
    if (signature[0] != 'D' || signature[1] != 'W' || signature[2] != 'B')
    {
        std::cout << "Error: invalid signature, not an LBR file?" << std::endl;
        return -1;
    }
    std::string count = readUntil(0x20);
    seekCurrent(1); // cr
    entry_count = atoi(count.c_str());

    //Debug_printv("signature[%s] count[%s] entry_count[%d]", signature.c_str(), count.c_str(), entry_count);

    for (int i = 0; i < entry_count; ++i)
    {
        std::string filename = readUntil(0x0D);
        std::string type = readUntil(0x0D);
        seekCurrent(1); // space
        std::string size = readUntil(0x20);
        seekCurrent(1); // cr

        //Debug_printv("i[%d] filename[%s] type[%s] size[%s]", i, filename.c_str(), type.c_str(), size.c_str());

        // Add Entry to array
        Entry e;
        e.filename = filename;
        e.size = atoi(size.c_str());
        e.type = decodeType(type);
        entries.push_back(e);

        //Debug_printv("i[%d] filename[%s] type[%s] size[%d]", i, e.filename.c_str(), e.type.c_str(), e.size);
    }

    // Calculate offset for start of entry
    uint32_t offset = _position;
    for (auto &e : entries)
    {
        e.offset = offset;
            offset += e.size;

        //Debug_printv("name[%s] type[%s] size[%d] offset[%d]", e.filename.c_str(), e.type.c_str(), e.size, e.offset);
    }

    return entry_count;
}


bool LBRMStream::seekEntry(std::string filename)
{
    size_t index = 0;
    mstr::replaceAll(filename, "\\", "/");
    bool wildcard = (mstr::contains(filename, "*") || mstr::contains(filename, "?"));

    // Read Directory Entries
    if (filename.size())
    {
        while (seekEntry(index))
        {
            std::string entryFilename = mstr::format("%.16s", entry.filename.c_str());
            mstr::replaceAll(entryFilename, "/", "\\");
            mstr::trim(entryFilename);
            entryFilename = mstr::toUTF8(entryFilename);

            Debug_printv("filename[%s] entry.filename[%s]", filename.c_str(), entryFilename.c_str());

            if (filename == entryFilename) // Match exact
            {
                return true;
            }
            else if (wildcard) // Wildcard Match
            {
                if (filename == "*") // Match first PRG
                {
                    filename = entryFilename;
                    return true;
                }
                else if (mstr::compare(filename, entryFilename)) // X?XX?X* Wildcard match
                {
                    return true;
                }
            }

            index++;
        }
    }

    entry.filename[0] = '\0';

    return false;
}

bool LBRMStream::seekEntry(uint16_t index)
{
    if ( index < entries.size() )
    {
        entry = entries[index];
        entry_index = index + 1;
        return true;
    }

    return false;
}

uint32_t LBRMStream::readFile(uint8_t *buf, uint32_t size)
{
    uint32_t bytesRead = 0;

    if (_position < 2)
    {
        // Debug_printv("position[%d] load00[%d] load01[%d]", _position, _load_address[0], _load_address[1]);

        buf[0] = _load_address[_position];
        bytesRead = size;
        // if ( size > 1 )
        // {
        //     buf[0] = m_load_address[0];
        //     buf[1] = m_load_address[1];
        //     bytesRead += containerStream->read(buf, size);
        // }
    }
    else
    {
        bytesRead += containerStream->read(buf, size);
    }

    return bytesRead;
}

bool LBRMStream::seekPath(std::string path)
{
    // Implement this to skip a queue of file streams to start of file by name
    // this will cause the next read to return bytes of 'path'
    seekCalled = true;

    entry_index = 0;

    // call image method to obtain file bytes here, return true on success:
    if (seekEntry(path))
    {
        // auto entry = containerImage->entry;
        auto type = entry.type.c_str();
        size_t data_offset = entry.offset;
        Debug_printv("filename [%.16s] type[%s] start_address[%zu] end_address[%zu] data_offset[%zu]", entry.filename, type, data_offset);

        // Calculate file size
        _size = entry.size;

        // Set position to beginning of file
        _position = 0;
        containerStream->seek(data_offset);

        Debug_printv("File Size: size[%d] available[%d] position[%d]", _size, available(), _position);

        return true;
    }
    else
    {
        Debug_printv("Not found! [%s]", path.c_str());
    }

    return false;
};

/********************************************************
 * File implementations
 ********************************************************/

bool LBRMFile::isDirectory()
{
    // Debug_printv("pathInStream[%s]", pathInStream.c_str());
    if (pathInStream == "")
        return true;
    else
        return false;
};

bool LBRMFile::rewindDirectory()
{
    dirIsOpen = true;
    Debug_printv("streamFile->url[%s]", streamFile->url.c_str());
    auto image = ImageBroker::obtain<LBRMStream>(streamFile->url);
    if (image == nullptr)
        Debug_printv("image pointer is null");

    image->resetEntryCounter();

    // Read Header
    image->seekHeader();

    // Set Media Info Fields
    media_header = mstr::format("%.16s", image->header.disk_name.c_str());
    media_id = image->header.id_dos;
    media_blocks_free = 0;
    media_block_size = image->block_size;
    media_image = name;
    // mstr::toUTF8(media_image);

    Debug_printv("media_header[%s] media_id[%s] media_blocks_free[%d] media_block_size[%d] media_image[%s]", media_header.c_str(), media_id.c_str(), media_blocks_free, media_block_size, media_image.c_str());

    return true;
}

MFile *LBRMFile::getNextFileInDir()
{

    if (!dirIsOpen)
        rewindDirectory();

    // Get entry pointed to by containerStream
    auto image = ImageBroker::obtain<LBRMStream>(streamFile->url);

    if (image->seekNextImageEntry())
    {
        std::string fileName = mstr::format("%.16s", image->entry.filename.c_str());
        mstr::replaceAll(fileName, "/", "\\");
        mstr::trim(fileName);
        //Debug_printv( "entry[%s]", (streamFile->url + "/" + fileName).c_str() );
        auto file = MFSOwner::File(streamFile->url + "/" + fileName);
        file->extension = image->entry.type;
        //Debug_printv("entry[%s] ext[%s]", fileName.c_str(), file->extension.c_str());
        return file;
    }
    else
    {
        // Debug_printv( "END OF DIRECTORY");
        dirIsOpen = false;
        return nullptr;
    }
}

uint32_t LBRMFile::size()
{
    // Debug_printv("[%s]", streamFile->url.c_str());
    // use LBR to get size of the file in image
    auto entry = ImageBroker::obtain<LBRMStream>(streamFile->url)->entry;

    return entry.size;
}
