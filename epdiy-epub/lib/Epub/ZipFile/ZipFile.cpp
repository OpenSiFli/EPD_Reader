#ifndef UNIT_TEST
#include <rtdbg.h>
#else
#define LOG_E(args...)
#define LOG_I(args...)
#endif
#include "ZipFile.h"

#include "miniz.h"

#define TAG "ZIP"

// read a file from the zip file allocating the required memory for the data
uint8_t *ZipFile::read_file_to_memory(const char *filename, size_t *size)
{
  // open up the epub file using miniz
  mz_zip_archive zip_archive;
  memset(&zip_archive, 0, sizeof(zip_archive));
  bool status = mz_zip_reader_init_file(&zip_archive, m_filename.c_str(), 0);
  if (!status)
  {
    ulog_e(TAG, "mz_zip_reader_init_file() failed!\n");
    ulog_e(TAG, "Error %s\n", mz_zip_get_error_string(zip_archive.m_last_error));
    return nullptr;
  }
  // find the file
  mz_uint32 file_index = 0;
  if (!mz_zip_reader_locate_file_v2(&zip_archive, filename, nullptr, 0, &file_index))
  {
    ulog_e(TAG, "Could not find file %s", filename);
    mz_zip_reader_end(&zip_archive);
    return nullptr;
  }
  // get the file size - we do this all manually so we can add a null terminator to any strings
  mz_zip_archive_file_stat file_stat;
  if (!mz_zip_reader_file_stat(&zip_archive, file_index, &file_stat))
  {
    ulog_e(TAG, "mz_zip_reader_file_stat() failed!\n");
    ulog_e(TAG, "Error %s\n", mz_zip_get_error_string(zip_archive.m_last_error));
    mz_zip_reader_end(&zip_archive);
    return nullptr;
  }
  // allocate memory for the file
  size_t file_size = file_stat.m_uncomp_size;
  uint8_t *file_data = (uint8_t *)calloc(file_size + 1, 1);
  if (!file_data)
  {
    ulog_e(TAG, "Failed to allocate memory for %s\n", file_stat.m_filename);
    mz_zip_reader_end(&zip_archive);
    return nullptr;
  }
  // read the file
  status = mz_zip_reader_extract_to_mem(&zip_archive, file_index, file_data, file_size, 0);
  if (!status)
  {
    ulog_e(TAG, "mz_zip_reader_extract_to_mem() failed!\n");
    ulog_e(TAG, "Error %s\n", mz_zip_get_error_string(zip_archive.m_last_error));
    free(file_data);
    mz_zip_reader_end(&zip_archive);
    return nullptr;
  }
  // Close the archive, freeing any resources it was using
  mz_zip_reader_end(&zip_archive);
  // return the size if required
  if (size)
  {
    *size = file_size;
  }
  return file_data;
}
bool ZipFile::read_file_to_file(const char *filename, const char *dest)
{
  mz_zip_archive zip_archive;
  memset(&zip_archive, 0, sizeof(zip_archive));
  bool status = mz_zip_reader_init_file(&zip_archive, m_filename.c_str(), 0);
  if (!status)
  {
    ulog_e(TAG, "mz_zip_reader_init_file() failed!\n");
    ulog_e(TAG, "Error %s\n", mz_zip_get_error_string(zip_archive.m_last_error));
    return false;
  }
  // Run through the archive and find the requiested file
  for (int i = 0; i < (int)mz_zip_reader_get_num_files(&zip_archive); i++)
  {
    mz_zip_archive_file_stat file_stat;
    if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat))
    {
      ulog_e(TAG, "mz_zip_reader_file_stat() failed!\n");
      ulog_e(TAG, "Error %s\n", mz_zip_get_error_string(zip_archive.m_last_error));
      mz_zip_reader_end(&zip_archive);
      return false;
    }
    // is this the file we're looking for?
    if (strcmp(filename, file_stat.m_filename) == 0)
    {
      ulog_i(TAG, "Extracting %s\n", file_stat.m_filename);
      mz_zip_reader_extract_file_to_file(&zip_archive, file_stat.m_filename, dest, 0);
      mz_zip_reader_end(&zip_archive);
      return true;
    }
  }
  mz_zip_reader_end(&zip_archive);
  return false;
}
