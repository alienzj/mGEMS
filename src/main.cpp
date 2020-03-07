#include <exception>
#include <iostream>
#include <cstring>

#include "telescope.hpp"
#include "cxxargs.hpp"

#include "file.hpp"
#include "assign_reads.h"
#include "extract_bin.h"
#include "mGEMS.h"

void ParseExtract(int argc, char* argv[], cxxargs::Arguments &args) {
  args.add_short_argument<std::string>('1', "Paired-end reads, first strand.");
  args.add_short_argument<std::string>('2', "Paired-end reads, second strand.");
  args.add_short_argument<std::string>('o', "Output directory.");
  args.add_long_argument<std::vector<std::string>>("bins", "Comma-separated list of bins to extract from the paired-end reads.");
  args.set_not_required("bins");
  args.set_not_required('o');

  args.parse(argc, argv);
}

void ParseBin(int argc, char* argv[], cxxargs::Arguments &args) {
  args.add_long_argument<std::vector<std::string>>("themisto-alns", "Comma-separated list of paired-end alignments from Themisto.");
  args.add_short_argument<std::string>('o', "Output directory (must exist before running).");
  args.add_short_argument<std::string>('a', "Relative abundances estimates from mSWEEP.");
  args.add_long_argument<std::string>("merge-mode", "How to merge paired-end alignments from Themisto (default: intersection).", "intersection");
  args.add_long_argument<std::string>("groups", "Which reference groups to bin reads to (default: all).");
  args.add_short_argument<long double>('q', "Tuning parameter for the binning thresholds (default: 1.0).", (long double)1);
  args.set_not_required("groups");

  args.parse(argc, argv);
}

void Extract(const std::vector<std::vector<uint32_t>> &bins, const std::vector<std::string> &target_groups, const cxxargs::Arguments &args) {
  uint32_t n_out_groups = bins.size();
  for (uint32_t i = 0; i < n_out_groups; ++i) {
    File::In istrand_1(args.value<std::string>('1'));
    File::In istrand_2(args.value<std::string>('2'));
    File::Out ostrand_1(args.value<std::string>('o') + target_groups[i] + "_1.fastq");
    File::Out ostrand_2(args.value<std::string>('o') + target_groups[i] + "_2.fastq");
    mGEMS::ExtractBin(bins[i], &ostrand_1.stream(), &ostrand_2.stream(), &istrand_1.stream(), &istrand_2.stream());
  }
}

void ReadAndExtract(cxxargs::Arguments &args) {
  uint32_t n_bins = args.value<std::vector<std::string>>("bins").size();
  std::vector<std::vector<uint32_t>> bins;
  std::vector<std::string> target_groups(n_bins);
  for (uint32_t i = 0; i < n_bins; ++i) {
    File::In istream(args.value<std::vector<std::string>>("bins")[i]);
    bins.emplace_back(mGEMS::ReadBin(istream.stream()));
  }
  Extract(bins, target_groups, args);
}

void Bin(const cxxargs::Arguments &args, bool extract_bins) {
  std::vector<std::string> groups;
  std::vector<long double> abundances;
  File::In msweep_abundances(args.value<std::string>('a'));
  uint32_t n_refs = mGEMS::ReadAbundances(msweep_abundances.stream(), &abundances, &groups);
  
  std::vector<std::istream*> themisto_alns;
  for (uint32_t i = 0; i < args.value<std::vector<std::string>>("themisto-alns").size(); ++i) {
    File::In themisto_aln(args.value<std::vector<std::string>>("themisto-alns")[i]);
    themisto_alns.push_back(&themisto_aln.stream());
  }
  
  ThemistoAlignment aln;
  ReadThemisto(get_mode(args.value<std::string>("merge-mode")), n_refs, themisto_alns, &aln);
  
  File::In probs_file(args.value<std::string>('p'));
  std::vector<std::string> target_groups = args.value<std::vector<std::string>>("groups");
  const std::vector<std::vector<uint32_t>> &bins = mGEMS::Bin(aln, args.value<long double>('q'), abundances, groups, probs_file.stream(), &target_groups);
  if (!extract_bins) {
    uint32_t n_bins = bins.size();
    for (uint32_t i = 0; i < n_bins; ++i) {
      File::Out of(args.value<std::string>('o') + '/' + target_groups[i] + "_reads.txt");
      mGEMS::WriteBin(bins[i], of.stream());
    }
  } else {
    Extract(bins, target_groups, args);
  }
}

int main(int argc, char* argv[]) {
  cxxargs::Arguments args("mGEMS", "Usage: type --help");
  try {
    std::cerr << "Parsing arguments" << std::endl;
    if (argc < 2) {
      std::cerr << args.help() << std::endl;
      return 0;
    } else if (strcmp(argv[1], "bin")) {
      ParseBin(argc, argv, args);
      Bin(args, false);
    } else if (strcmp(argv[1], "extract")) {
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
    std::cerr << "Parsing arguments failed:\n"
	      << "\t" << e.what()
	      << "\nexiting\n";
    return 1;
  }

  return 0;
}
