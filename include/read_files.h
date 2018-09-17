#ifndef READ_FILES_H
#define READ_FILES_H

#include <string>

void read_sam(const std::string &sam_path, const std::string &ec_path, const std::string &outfile, const std::string &probs_path, const std::string &abundances_path);

#endif
  