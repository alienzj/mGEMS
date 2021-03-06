#include <dirent.h>

#include <exception>
#include <iostream>
#include <cstring>

#include "telescope.hpp"
#include "cxxargs.hpp"

#include "file.hpp"
#include "bin_reads.h"
#include "extract_bin.h"
#include "mGEMS.h"

uint32_t CountLines(std::istream &stream) {
  std::string line;
  uint32_t n_lines = 0;
  while(std::getline(stream, line)) {
    ++n_lines;
  }
  return n_lines;
}

void ParseExtract(int argc, char* argv[], cxxargs::Arguments &args) {
  args.add_short_argument<std::vector<std::string>>('r', "Input reads (comma separated list)");
  args.add_short_argument<std::string>('o', "Output directory.");
  args.add_long_argument<std::vector<std::string>>("bins", "Comma-separated list of bins to extract from the paired-end reads.");
  args.add_long_argument<bool>("compress", "Compress extracted reads with zlib (.gz extension, default: true)", true);
  args.set_not_required("bins");
  args.set_not_required('o');

  args.parse(argc, argv);
}

void ParseBin(int argc, char* argv[], cxxargs::Arguments &args) {
  args.add_long_argument<std::vector<std::string>>("themisto-alns", "Comma-separated list of paired-end alignments from Themisto.");
  args.add_short_argument<std::string>('o', "Output directory (must exist before running).");
  args.add_short_argument<std::string>('a', "Relative abundances estimates from mSWEEP.");
  args.add_long_argument<std::string>("probs", "Posterior probabilities from mSWEEP.");
  args.add_long_argument<std::string>("merge-mode", "How to merge paired-end alignments from Themisto (default: intersection).", "intersection");
  args.add_long_argument<std::vector<std::string>>("groups", "Which reference groups to bin reads to (default: all).");
  args.add_long_argument<long double>("min-abundance", "Bin only the groups that have a relative abundance higher than this value (optional).");
  args.add_short_argument<long double>('q', "Tuning parameter for the binning thresholds (default: 1.0).", (long double)1);
  args.add_long_argument<std::string>("index", "Themisto pseudoalignment index directory.");
  args.set_not_required("groups");
  args.set_not_required("min-abundance");

  args.parse(argc, argv);
}

void Extract(const std::vector<std::vector<uint32_t>> &bins, const std::vector<std::string> &target_groups, const cxxargs::Arguments &args) {
  uint32_t n_out_groups = bins.size();
  uint8_t n_strands = args.value<std::vector<std::string>>('r').size();
  std::vector<File::In> in_strands(n_strands);
  std::vector<File::Out> out_strands(n_strands);
  for (uint32_t i = 0; i < n_out_groups; ++i) {
    for (uint8_t j = 0; j < n_strands; ++j) {
      in_strands[j].open(args.value<std::vector<std::string>>('r')[j]);
      std::string out_name = args.value<std::string>('o') + "/" + target_groups[i] + "_" + std::to_string(j + 1) + ".fastq";
      if (args.value<bool>("compress")) {
	out_name += ".gz";
	out_strands[j].open_compressed(out_name);
      } else {
	out_strands[j].open(out_name);
      }
    }
    mGEMS::ExtractBin(bins[i], in_strands, &out_strands);
  }
}

void ReadAndExtract(cxxargs::Arguments &args) {
  DIR* dir = opendir(args.value<std::string>('o').c_str());
  if (dir) {
    closedir(dir);
  } else {
    throw std::runtime_error("Directory " + args.value<std::string>('o') + " does not seem to exist.");
  }
  uint32_t n_bins = args.value<std::vector<std::string>>("bins").size();
  std::vector<std::vector<uint32_t>> bins;
  std::vector<std::string> target_groups(n_bins);
  for (uint32_t i = 0; i < n_bins; ++i) {
    File::In istream(args.value<std::vector<std::string>>("bins")[i]);
    bins.emplace_back(mGEMS::ReadBin(istream.stream()));
    std::string out_name = args.value<std::vector<std::string>>("bins")[i];
    if (out_name.find(".") != std::string::npos) {
      out_name.erase(out_name.find("."), out_name.size());
    }
    if (out_name.rfind("/") != std::string::npos) {
      out_name.erase(0, out_name.rfind("/") + 1);
    }
    target_groups[i] = out_name;
  }
  Extract(bins, target_groups, args);
}

void FilterTargetGroups(const std::vector<std::string> &group_names, const std::vector<long double> &abundances, const long double min_abundance, std::vector<std::string> *target_groups) {
  uint32_t n_groups = group_names.size();
  for (uint32_t i = 0; i < n_groups; ++i) {
    if (abundances[i] < min_abundance && std::find(target_groups->begin(), target_groups->end(), group_names[i]) != target_groups->end()) {
      target_groups->erase(std::find(target_groups->begin(), target_groups->end(), group_names[i]));
    }
  }
}

void Bin(const cxxargs::Arguments &args, bool extract_bins) {
  DIR* dir = opendir(args.value<std::string>('o').c_str());
  if (dir) {
    closedir(dir);
  } else {
    throw std::runtime_error("Directory " + args.value<std::string>('o') + " does not seem to exist.");
  }
  dir = opendir(args.value<std::string>("index").c_str());
  if (dir) {
    closedir(dir);
  } else {
    throw std::runtime_error("Themisto pseudoalignment index directory " + args.value<std::string>('o') + " does not seem to exist.");
  }
  std::vector<std::string> groups;
  std::vector<long double> abundances;
  File::In msweep_abundances(args.value<std::string>('a'));
  mGEMS::ReadAbundances(msweep_abundances.stream(), &abundances, &groups);
  
  std::vector<std::istream*> themisto_alns;
  for (uint32_t i = 0; i < args.value<std::vector<std::string>>("themisto-alns").size(); ++i) {
    File::In(args.value<std::vector<std::string>>("themisto-alns")[i]);
    themisto_alns.emplace_back(new bxz::ifstream(args.value<std::vector<std::string>>("themisto-alns")[i]));
  }

  std::ifstream themisto_index(args.value<std::string>("index") + "/coloring-names.txt");
  uint32_t n_refs = CountLines(themisto_index);
  themisto_index.close();
  ThemistoAlignment aln;
  ReadThemisto(get_mode(args.value<std::string>("merge-mode")), n_refs, themisto_alns, &aln);

  File::In probs_file(args.value<std::string>("probs"));
  std::vector<std::string> target_groups;
  if (args.is_initialized("groups")) {
    target_groups = args.value<std::vector<std::string>>("groups");
  } else {
    target_groups = groups;
  }
  if (args.is_initialized("min-abundance")) {
    FilterTargetGroups(groups, abundances, args.value<long double>("min-abundance"), &target_groups);
  }

  const std::vector<std::vector<uint32_t>> &bins = mGEMS::Bin(aln, args.value<long double>('q'), abundances, groups, probs_file.stream(), &target_groups);
  if (!extract_bins) {
    uint32_t n_bins = bins.size();
    for (uint32_t i = 0; i < n_bins; ++i) {
      File::Out of(args.value<std::string>('o') + '/' + target_groups[i] + ".bin");
      mGEMS::WriteBin(bins[i], of.stream());
    }
  } else {
    Extract(bins, target_groups, args);
  }
}

int main(int argc, char* argv[]) {
  cxxargs::Arguments args("mGEMS", "Usage: mGEMS -r <input-reads_1>,<input-reads_2> --themisto-alns <input-reads_1 pseudoalignments>,<input-reads_2 pseudoalignments> -o <output directory> --probs <posterior probabilities> -a <abundance estimates> --index <Themisto index> --groups <group names to extract (optional)>");
  try {
    if (argc < 2) {
      std::cerr << args.help() << std::endl;
      return 0;
    } else if (strcmp(argv[1], "bin") == 0) {
      ParseBin(argc, argv, args);
      Bin(args, false);
    } else if (strcmp(argv[1], "extract") == 0) {
      ParseExtract(argc, argv, args);
      if (!args.is_initialized("bins")) {
	throw std::runtime_error("Bins must be specified.");
      }
      if (!args.is_initialized('o')) {
	throw std::runtime_error("Output directory not given.");
      }
      ReadAndExtract(args);
    } else {
      ParseBin(argc, argv, args);
      ParseExtract(argc, argv, args);
      Bin(args, true);
    }
  } catch (std::exception &e) {
    std::cerr << "Error:"
	      << "\t" << e.what()
	      << "\nexiting\n";
    return 1;
  }

  return 0;
}
