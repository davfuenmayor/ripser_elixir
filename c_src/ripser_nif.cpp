#include <algorithm>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <mutex>
#include <numeric>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "erl_nif.h"

#ifndef RIPSER_CPP_INCLUDE
#define RIPSER_CPP_INCLUDE "vendor/ripser.cpp"
#endif

#define main ripser_cli_main
#define static
#include RIPSER_CPP_INCLUDE
#undef static
#undef main

namespace {

struct interval {
	double birth;
	bool finite_death;
	double death;
};

typedef std::map<int, std::vector<interval>> intervals_by_dim;

enum class nif_format {
	distance,
	lower_distance,
	upper_distance,
	point_cloud,
	sparse,
	dipha,
	binary
};

struct options {
	nif_format format = nif_format::distance;
	index_t dim = 1;
	value_t threshold = std::numeric_limits<value_t>::max();
	float ratio = 1;
	coefficient_t modulus = 2;
};

std::mutex ripser_mutex;

bool get_atom(ErlNifEnv* env, ERL_NIF_TERM term, std::string& value) {
	unsigned len;
	if (!enif_get_atom_length(env, term, &len, ERL_NIF_LATIN1)) return false;
	std::vector<char> buffer(len + 1);
	if (!enif_get_atom(env, term, buffer.data(), buffer.size(), ERL_NIF_LATIN1)) return false;
	value.assign(buffer.data(), len);
	return true;
}

bool get_string(ErlNifEnv* env, ERL_NIF_TERM term, std::string& value) {
	ErlNifBinary binary;
	if (enif_inspect_binary(env, term, &binary)) {
		value.assign(reinterpret_cast<const char*>(binary.data), binary.size);
		return true;
	}
	return get_atom(env, term, value);
}

bool parse_format(ErlNifEnv* env, ERL_NIF_TERM term, nif_format& format) {
	std::string value;
	if (!get_string(env, term, value)) return false;

	for (char& c : value)
		if (c == '-') c = '_';

	if (value == "distance") {
		format = nif_format::distance;
	} else if (value == "lower_distance" || value == "lower") {
		format = nif_format::lower_distance;
	} else if (value == "upper_distance" || value == "upper") {
		format = nif_format::upper_distance;
	} else if (value == "point_cloud" || value == "point") {
		format = nif_format::point_cloud;
	} else if (value == "sparse") {
		format = nif_format::sparse;
	} else if (value == "dipha") {
		format = nif_format::dipha;
	} else if (value == "binary") {
		format = nif_format::binary;
	} else {
		return false;
	}
	return true;
}

bool get_double(ErlNifEnv* env, ERL_NIF_TERM term, double& value) {
	if (enif_get_double(env, term, &value)) return true;

	long integer;
	if (enif_get_long(env, term, &integer)) {
		value = static_cast<double>(integer);
		return true;
	}
	return false;
}

bool parse_options(ErlNifEnv* env, ERL_NIF_TERM opts_term, options& opts, std::string& error) {
	ERL_NIF_TERM list = opts_term;
	ERL_NIF_TERM head, tail;

	while (enif_get_list_cell(env, list, &head, &tail)) {
		const ERL_NIF_TERM* tuple;
		int arity;
		if (!enif_get_tuple(env, head, &arity, &tuple) || arity != 2) {
			error = "options must be a keyword list";
			return false;
		}

		std::string key;
		if (!get_atom(env, tuple[0], key)) {
			error = "option keys must be atoms";
			return false;
		}

		if (key == "format") {
			if (!parse_format(env, tuple[1], opts.format)) {
				error = "unknown format";
				return false;
			}
		} else if (key == "dim") {
			long dim;
			if (!enif_get_long(env, tuple[1], &dim) || dim < 0) {
				error = "dim must be a non-negative integer";
				return false;
			}
			opts.dim = static_cast<index_t>(dim);
		} else if (key == "threshold") {
			std::string atom;
			if (get_atom(env, tuple[1], atom) && atom == "infinity") {
				opts.threshold = std::numeric_limits<value_t>::max();
			} else {
				double threshold;
				if (!get_double(env, tuple[1], threshold)) {
					error = "threshold must be a number or :infinity";
					return false;
				}
				opts.threshold = static_cast<value_t>(threshold);
			}
		} else if (key == "ratio") {
			double ratio;
			if (!get_double(env, tuple[1], ratio)) {
				error = "ratio must be a number";
				return false;
			}
			opts.ratio = static_cast<float>(ratio);
		} else if (key == "modulus") {
			long modulus;
			if (!enif_get_long(env, tuple[1], &modulus) || modulus < 2 ||
			    modulus > std::numeric_limits<coefficient_t>::max() ||
			    !is_prime(static_cast<coefficient_t>(modulus))) {
				error = "modulus must be a prime integer";
				return false;
			}
			opts.modulus = static_cast<coefficient_t>(modulus);
		} else {
			error = "unknown option";
			return false;
		}

		list = tail;
	}

	if (!enif_is_empty_list(env, list)) {
		error = "options must be a keyword list";
		return false;
	}
	return true;
}

std::string trim(const std::string& value) {
	const std::string whitespace = " \t\r\n";
	const size_t first = value.find_first_not_of(whitespace);
	if (first == std::string::npos) return "";
	const size_t last = value.find_last_not_of(whitespace);
	return value.substr(first, last - first + 1);
}

double parse_number(const std::string& value) {
	char* end = nullptr;
	errno = 0;
	const double result = std::strtod(value.c_str(), &end);
	if (errno || end == value.c_str()) throw std::runtime_error("failed to parse interval");
	return result;
}

intervals_by_dim parse_intervals(const std::string& output) {
	intervals_by_dim intervals;
	std::istringstream stream(output);
	std::string line;
	int current_dim = -1;

	while (std::getline(stream, line)) {
		line = trim(line);
		const std::string prefix = "persistence intervals in dim ";
		if (line.compare(0, prefix.size(), prefix) == 0) {
			std::string dim = line.substr(prefix.size());
			if (!dim.empty() && dim.back() == ':') dim.pop_back();
			current_dim = static_cast<int>(parse_number(dim));
			intervals[current_dim];
			continue;
		}

		if (current_dim < 0 || line.empty() || line.front() != '[') continue;

		const size_t comma = line.find(',');
		const size_t close = line.find(')', comma == std::string::npos ? 0 : comma);
		if (comma == std::string::npos || close == std::string::npos) {
			throw std::runtime_error("failed to parse interval");
		}

		interval parsed;
		parsed.birth = parse_number(line.substr(1, comma - 1));

		std::string death = trim(line.substr(comma + 1, close - comma - 1));
		parsed.finite_death = !death.empty();
		parsed.death = parsed.finite_death ? parse_number(death) : 0;
		intervals[current_dim].push_back(parsed);
	}

	return intervals;
}

void run_ripser(std::istream& input, const options& opts) {
	if (opts.format == nif_format::sparse) {
		sparse_distance_matrix dist = read_sparse_distance_matrix(input);
		std::cout << "sparse distance matrix with " << dist.size() << " points and "
		          << dist.num_edges << "/" << (dist.size() * (dist.size() - 1)) / 2 << " entries"
		          << std::endl;

		ripser<sparse_distance_matrix>(std::move(dist), opts.dim, opts.threshold, opts.ratio,
		                               opts.modulus)
		    .compute_barcodes();
		return;
	}

	if (opts.format == nif_format::point_cloud &&
	    opts.threshold < std::numeric_limits<value_t>::max()) {
		sparse_distance_matrix dist(read_point_cloud(input), opts.threshold);
		ripser<sparse_distance_matrix>(std::move(dist), opts.dim, opts.threshold, opts.ratio,
		                               opts.modulus)
		    .compute_barcodes();
		return;
	}

	file_format format = DISTANCE_MATRIX;
	switch (opts.format) {
	case nif_format::lower_distance:
		format = LOWER_DISTANCE_MATRIX;
		break;
	case nif_format::upper_distance:
		format = UPPER_DISTANCE_MATRIX;
		break;
	case nif_format::point_cloud:
		format = POINT_CLOUD;
		break;
	case nif_format::dipha:
		format = DIPHA;
		break;
	case nif_format::binary:
		format = BINARY;
		break;
	default:
		format = DISTANCE_MATRIX;
		break;
	}

	compressed_lower_distance_matrix dist = read_file(input, format);

	value_t min = std::numeric_limits<value_t>::infinity();
	value_t max = -std::numeric_limits<value_t>::infinity();
	value_t max_finite = max;
	int num_edges = 0;

	value_t enclosing_radius = std::numeric_limits<value_t>::infinity();
	if (opts.threshold == std::numeric_limits<value_t>::max()) {
		for (size_t i = 0; i < dist.size(); ++i) {
			value_t r_i = -std::numeric_limits<value_t>::infinity();
			for (size_t j = 0; j < dist.size(); ++j) r_i = std::max(r_i, dist(i, j));
			enclosing_radius = std::min(enclosing_radius, r_i);
		}
	}

	for (auto d : dist.distances) {
		min = std::min(min, d);
		max = std::max(max, d);
		if (d != std::numeric_limits<value_t>::infinity()) max_finite = std::max(max_finite, d);
		if (d <= opts.threshold) ++num_edges;
	}
	std::cout << "value range: [" << min << "," << max_finite << "]" << std::endl;

	if (opts.threshold == std::numeric_limits<value_t>::max()) {
		std::cout << "distance matrix with " << dist.size()
		          << " points, using threshold at enclosing radius " << enclosing_radius
		          << std::endl;
		ripser<compressed_lower_distance_matrix>(std::move(dist), opts.dim, enclosing_radius,
		                                         opts.ratio, opts.modulus)
		    .compute_barcodes();
	} else {
		std::cout << "sparse distance matrix with " << dist.size() << " points and " << num_edges
		          << "/" << (dist.size() * (dist.size() - 1)) / 2 << " entries" << std::endl;
		ripser<sparse_distance_matrix>(
		    sparse_distance_matrix(std::move(dist), opts.threshold), opts.dim, opts.threshold,
		    opts.ratio, opts.modulus)
		    .compute_barcodes();
	}
}

ERL_NIF_TERM make_binary(ErlNifEnv* env, const std::string& value) {
	ERL_NIF_TERM term;
	unsigned char* data = enif_make_new_binary(env, value.size(), &term);
	if (!value.empty()) std::memcpy(data, value.data(), value.size());
	return term;
}

ERL_NIF_TERM make_error(ErlNifEnv* env, const std::string& reason) {
	return enif_make_tuple2(env, enif_make_atom(env, "error"), make_binary(env, reason));
}

ERL_NIF_TERM make_interval(ErlNifEnv* env, const interval& value) {
	ERL_NIF_TERM death =
	    value.finite_death ? enif_make_double(env, value.death) : enif_make_atom(env, "infinity");
	return enif_make_tuple2(env, enif_make_double(env, value.birth), death);
}

ERL_NIF_TERM make_interval_map(ErlNifEnv* env, const intervals_by_dim& intervals) {
	ERL_NIF_TERM map = enif_make_new_map(env);

	for (const auto& dim_intervals : intervals) {
		ERL_NIF_TERM list = enif_make_list(env, 0);
		for (auto it = dim_intervals.second.rbegin(); it != dim_intervals.second.rend(); ++it) {
			list = enif_make_list_cell(env, make_interval(env, *it), list);
		}
		enif_make_map_put(env, map, enif_make_int(env, dim_intervals.first), list, &map);
	}

	return map;
}

ERL_NIF_TERM make_result(ErlNifEnv* env, const std::string& output,
                         const intervals_by_dim& intervals) {
	ERL_NIF_TERM result = enif_make_new_map(env);
	enif_make_map_put(env, result, enif_make_atom(env, "output"), make_binary(env, output),
	                  &result);
	enif_make_map_put(env, result, enif_make_atom(env, "intervals"), make_interval_map(env, intervals),
	                  &result);
	return enif_make_tuple2(env, enif_make_atom(env, "ok"), result);
}

ERL_NIF_TERM compute_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
	if (argc != 2) return enif_make_badarg(env);

	ErlNifBinary input_binary;
	if (!enif_inspect_binary(env, argv[0], &input_binary)) return enif_make_badarg(env);

	options opts;
	std::string error;
	if (!parse_options(env, argv[1], opts, error)) return make_error(env, error);

	std::string input(reinterpret_cast<const char*>(input_binary.data), input_binary.size);
	std::istringstream input_stream(input);
	std::ostringstream output_stream;
	std::lock_guard<std::mutex> lock(ripser_mutex);
	std::streambuf* old_cout = std::cout.rdbuf(output_stream.rdbuf());

	try {
		run_ripser(input_stream, opts);
		std::cout.rdbuf(old_cout);
		const std::string output = output_stream.str();
		return make_result(env, output, parse_intervals(output));
	} catch (const std::exception& ex) {
		std::cout.rdbuf(old_cout);
		return make_error(env, ex.what());
	} catch (...) {
		std::cout.rdbuf(old_cout);
		return make_error(env, "unknown error");
	}
}

ErlNifFunc nif_funcs[] = {
    {"compute_nif", 2, compute_nif, 0},
};

} // namespace

ERL_NIF_INIT(Elixir.Ripser, nif_funcs, nullptr, nullptr, nullptr, nullptr)
