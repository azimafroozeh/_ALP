#include "alp.hpp"
#include "data.hpp"
#include "gtest/gtest.h"
#include <fstream>

/// ALP encoded size per vector  = bit_width + factor-idx + exponent-idx + ffor base;
double overhead_per_vector {static_cast<double>(8 + 8 + 8 + 64) / alp::config::VECTOR_SIZE};

///  ALP_RD Overhead encoded size
double alprd_overhead_per_vector {static_cast<double>(alp::config::MAX_RD_DICTIONARY_SIZE * 16) /
                                  alp::config::ROWGROUP_SIZE};

namespace test {
template <typename T>
void ALP_ASSERT(T original_val, T decoded_val) {
	if (original_val == 0.0 && std::signbit(original_val)) {
		ASSERT_EQ(decoded_val, 0.0);
		ASSERT_TRUE(std::signbit(decoded_val));
	} else if (std::isnan(original_val)) {
		ASSERT_TRUE(std::isnan(decoded_val));
	} else {
		ASSERT_EQ(original_val, decoded_val);
	}
}

} // namespace test
class alp_test : public ::testing::Test {
public:
	double*   dbl_arr {};
	double*   exc_arr {};
	uint16_t* rd_exc_arr {};
	uint16_t* pos_arr {};
	uint16_t* exc_c_arr {};
	int64_t*  ffor_arr {};
	int64_t*  unffor_arr {};
	int64_t*  base_arr {};
	int64_t*  encoded_arr {};
	double*   dec_dbl_arr {};
	double*   smp_arr {};
	uint64_t* ffor_right_arr {};
	uint16_t* ffor_left_arr {};
	uint64_t* right_arr {};
	uint16_t* left_arr {};
	uint64_t* unffor_right_arr {};
	uint16_t* unffor_left_arr {};
	double*   glue_arr {};

	alp::state state;

	alp::bw_t bit_width {};

	void SetUp() override {
		dbl_arr          = new double[alp::config::VECTOR_SIZE];
		exc_arr          = new double[alp::config::VECTOR_SIZE];
		rd_exc_arr       = new uint16_t[alp::config::VECTOR_SIZE];
		pos_arr          = new uint16_t[alp::config::VECTOR_SIZE];
		encoded_arr      = new int64_t[alp::config::VECTOR_SIZE];
		dec_dbl_arr      = new double[alp::config::VECTOR_SIZE];
		exc_c_arr        = new uint16_t[alp::config::VECTOR_SIZE];
		ffor_arr         = new int64_t[alp::config::VECTOR_SIZE];
		unffor_arr       = new int64_t[alp::config::VECTOR_SIZE];
		base_arr         = new int64_t[alp::config::VECTOR_SIZE];
		smp_arr          = new double[alp::config::VECTOR_SIZE];
		right_arr        = new uint64_t[alp::config::VECTOR_SIZE];
		left_arr         = new uint16_t[alp::config::VECTOR_SIZE];
		ffor_right_arr   = new uint64_t[alp::config::VECTOR_SIZE];
		ffor_left_arr    = new uint16_t[alp::config::VECTOR_SIZE];
		unffor_right_arr = new uint64_t[alp::config::VECTOR_SIZE];
		unffor_left_arr  = new uint16_t[alp::config::VECTOR_SIZE];
		glue_arr         = new double[alp::config::VECTOR_SIZE];
	}

	~alp_test() override {
		delete[] dbl_arr;
		delete[] exc_arr;
		delete[] rd_exc_arr;
		delete[] pos_arr;
		delete[] encoded_arr;
		delete[] dec_dbl_arr;
		delete[] exc_c_arr;
		delete[] ffor_arr;
		delete[] unffor_arr;
		delete[] base_arr;
		delete[] smp_arr;
		delete[] right_arr;
		delete[] left_arr;
		delete[] unffor_right_arr;
		delete[] unffor_left_arr;
	}

	template <typename PT>
	void test_column(const alp_bench::Column& column) {
		std::ifstream file(column.sample_csv_file_path, std::ios::in);
		if (!file) { throw std::runtime_error(column.sample_csv_file_path + " : " + strerror(errno)); }

		alp::state stt;
		size_t     tuples_count {alp::config::VECTOR_SIZE};
		size_t     rowgroup_offset {0};

		double      value_to_encode;
		std::string val_str;
		// keep storing values from the text file so long as data exists:
		size_t row_idx {0};
		while (file >> val_str) {
			value_to_encode  = std::stod(val_str);
			dbl_arr[row_idx] = value_to_encode;

			row_idx += 1;
		}

		// Init
		alp::encoder<PT>::init(dbl_arr, rowgroup_offset, tuples_count, smp_arr, stt);

		switch (stt.scheme) {
		case alp::SCHEME::ALP_RD: {
			alp::AlpRD<PT>::init(dbl_arr, rowgroup_offset, tuples_count, smp_arr, stt);

			alp::AlpRD<PT>::encode(dbl_arr, rd_exc_arr, pos_arr, exc_c_arr, right_arr, left_arr, stt);
			ffor::ffor(right_arr, ffor_right_arr, stt.right_bit_width, &stt.right_for_base);
			ffor::ffor(left_arr, ffor_left_arr, stt.left_bit_width, &stt.left_for_base);

			// Decode
			unffor::unffor(ffor_right_arr, unffor_right_arr, stt.right_bit_width, &stt.right_for_base);
			unffor::unffor(ffor_left_arr, unffor_left_arr, stt.left_bit_width, &stt.left_for_base);
			alp::AlpRD<PT>::decode(glue_arr, unffor_right_arr, unffor_left_arr, rd_exc_arr, pos_arr, exc_c_arr, stt);

			for (size_t i = 0; i < alp::config::VECTOR_SIZE; ++i) {
				auto l = dbl_arr[i];
				auto r = glue_arr[i];
				if (l != r) { std::cout << i << " | " << i << " r : " << r << " l : " << l << '\n'; }
				test::ALP_ASSERT(r, l);
			}

			break;
		}
		case alp::SCHEME::ALP: { // Encode
			alp::encoder<PT>::encode(dbl_arr, exc_arr, pos_arr, exc_c_arr, encoded_arr, stt);
			alp::encoder<PT>::analyze_ffor(encoded_arr, bit_width, base_arr);
			ffor::ffor(encoded_arr, ffor_arr, bit_width, base_arr);

			// Decode
			generated::falp::fallback::scalar::falp(reinterpret_cast<uint64_t*>(ffor_arr),
			                                        dec_dbl_arr,
			                                        bit_width,
			                                        reinterpret_cast<uint64_t*>(base_arr),
			                                        stt.fac,
			                                        stt.exp);
			alp::AlpDecode<PT>::patch_exceptions(dec_dbl_arr, exc_arr, pos_arr, exc_c_arr);

			auto exceptions_count = exc_c_arr[0];

			for (size_t i = 0; i < alp::config::VECTOR_SIZE; ++i) {
				test::ALP_ASSERT(dbl_arr[i], dec_dbl_arr[i]);
			}

			ASSERT_EQ(column.exceptions_count, exceptions_count);
			ASSERT_EQ(column.bit_width, bit_width);
		}
		}

		std::cout << "Testing ALP on one vector on dataset: " << column.name << " OK" << '\n';

		file.close();
	}
};

/// Test used for correctness of bitwidth and exceptions on the first vector of each dataset
TEST_F(alp_test, test_alp_double) {
	for (const auto& col : alp_bench::alp_dataset) {
		test_column<double>(col);
	}
}

/// Test used for correctness of bitwidth and exceptions on the first vector of generated data
TEST_F(alp_test, test_alp_on_generated) {
	for (const auto& col : alp_bench::generated_cols) {
		if (col.bit_width > 42) { continue; }
		test_column<double>(col);
	}
}

// Test used for correctness of bitwidth and exceptions on the first vector of edge_case data
TEST_F(alp_test, test_alp_on_edge_case) {
	for (const auto& col : alp_bench::edge_case) {
		test_column<double>(col);
	}
}
