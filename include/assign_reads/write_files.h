#ifndef WRITE_FILES_H
#define WRITE_FILES_H

#include <map>
#include <unordered_map>
#include <vector>
#include <string>

void write_reads(const std::unordered_map<long unsigned, std::vector<std::string>> &reads_in_ec,
		 const std::unordered_map<long unsigned, std::vector<bool>> &probs,
		 const std::vector<std::string> &ref_names,
		 const std::vector<short unsigned> &group_indices,
		 const std::string &outfile,
		 const bool gzip_output);
void write_ecs(const std::unordered_map<long unsigned, std::vector<std::string>> &reads_in_ec,
	       const std::string &outfile,
	       const bool gzip_output);
#endif