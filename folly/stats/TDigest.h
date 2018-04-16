/*
 * Copyright 2012-present Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <vector>

#include <folly/Range.h>

namespace folly {

/*
 * TDigests are a biased quantile estimator designed to estimate the values of
 * the quantiles of streaming data with high accuracy and low memory,
 * particularly for quantiles at the tails (p0.1, p1, p99, p99.9). See
 * https://github.com/tdunning/t-digest/blob/master/docs/t-digest-paper/histo.pdf
 * for an explanation of what the purpose of TDigests is, and how they work.
 *
 * There is a notable difference between the implementation here and the
 * implementation in the paper. In the paper, the recommended scaling function
 * for bucketing centroids is an arcsin function. The arcsin function provides
 * high accuracy for low memory, but comes at a relatively high compute cost.
 * A good choice algorithm has the following properties:
 *   - The value of the function k(0, delta) = 0, and k(1, delta) = delta.
 *     This is a requirement for any t-digest function.
 *   - The limit of the derivative of the function dk/dq at 0 is inf, and at
 *     1 is inf. This provides bias to improve accuracy at the tails.
 *   - For any q <= 0.5, dk/dq(q) = dk/dq(1-q). This ensures that the accuracy
 *     of upper and lower quantiles are equivalent.
 * As such, TDigest uses a sqrt function with these properties, which is faster
 * than arcsin. There is a small, but relatively negligible impact to accuracy
 * at the tail. In empirical tests, accuracy of the sqrt approach has been
 * adequate.
 */
class TDigest {
 public:
  explicit TDigest(size_t maxSize = 100) : maxSize_(maxSize), total_(0.0) {
    centroids_.reserve(maxSize);
  }

  /*
   * Returns a new TDigest constructed with values merged from the current
   * digest and the given sortedValues.
   */
  TDigest merge(Range<const double*> sortedValues) const;

  /*
   * Returns a new TDigest constructed with values merged from the given
   * digests.
   */
  static TDigest merge(Range<const TDigest*> digests);

  /*
   * Estimates the value of the given quantile.
   */
  double estimateQuantile(double q) const;

 private:
  struct Centroid {
   public:
    explicit Centroid(double mean = 0.0, double weight = 1.0)
        : mean_(mean), weight_(weight) {}

    inline double mean() const {
      return mean_;
    }

    inline double weight() const {
      return weight_;
    }

    inline void add(const Centroid& other);

    inline bool operator<(const Centroid& other) const {
      return mean() < other.mean();
    }

   private:
    double mean_;
    double weight_;
  };

  std::vector<Centroid> centroids_;
  size_t maxSize_;
  double total_;
};

} // namespace folly