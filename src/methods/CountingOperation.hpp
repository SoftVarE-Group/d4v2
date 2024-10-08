/*
 * d4
 * Copyright (C) 2020  Univ. Artois & CNRS
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <atomic>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/multiprecision/detail/default_ops.hpp>
#include <boost/multiprecision/gmp.hpp>

#include "DataBranch.hpp"
#include "src/exceptions/BadBehaviourException.hpp"
#include <3rdParty/glucose-3.0/mtl/Vec.h>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <format>
#include <fstream>
#include <sstream>
#include <thread>
#include <variant>

using namespace std::chrono_literals;
template <class... Ts> struct overloaded : Ts... {
  using Ts::operator()...;
};

namespace d4 {
template <class T, class U> class Operation;
template <typename T>
class PersistentNodesOperation : public Operation<T, size_t> {
private:
  ProblemManager *m_problem;
  struct AndNode {
    size_t id;
    std::vector<size_t> children;
  };
  struct OrNode {
    size_t id;
    DataBranch<size_t> b[2];
  };
  struct UnaryNode {
    size_t id;
    DataBranch<size_t> b;
  };
  enum NodeTypeID : char {
    True = 1,
    False = 2,
    And = 3,
    Or = 3,
    Arc = 4,
    ArcAnd = 4,
  };
  using Node = std::variant<AndNode, OrNode, UnaryNode>;
  std::vector<Node> out;
  std::atomic<std::vector<Node> *> to_write;
  std::atomic<bool> end;
  std::thread writer;
  std::atomic_bool idx;

  size_t index = 3;

  const size_t top = 1;
  const size_t bottom = 2;

public:
  PersistentNodesOperation() = delete;
  ~PersistentNodesOperation() {
    while (to_write.load() != nullptr) {
      std::this_thread::sleep_for(10ms);
    }
    to_write.exchange(new std::vector<Node>(std::move(out)));
    end.store(true);
    writer.join();
  }

  /**
     Constructor.

     @param[in] problem, allows to get information about the problem such as
     weights.
   */
  PersistentNodesOperation(ProblemManager *problem, std::string filename)
      : m_problem(problem), to_write(0), end(false) {
    writer = std::thread([&,filename] {
      std::cout << "Start writer" << std::endl;
      std::ofstream file =
          std::ofstream(filename, std::ios_base::out | std::ios_base::binary);
      boost::iostreams::filtering_streambuf<boost::iostreams::output> outbuf;
      outbuf.push(boost::iostreams::gzip_compressor());
      outbuf.push(file);
      std::ostream out(&outbuf);
      out << NodeTypeID::True << uint64_t(1);
      out << NodeTypeID::False << uint64_t(2);
      auto write_arc = [&](DataBranch<size_t> &b, size_t parent) {
        out << NodeTypeID::Arc << uint64_t(parent) << uint64_t(b.d)
            << (uint32_t)b.unitLits.size();
        for (auto i : b.unitLits) {
          if (i.sign()) {
            out << -int32_t(i.var());
          } else {
            out << int32_t(i.var());
          }
        }
      };
      auto try_write = [&](std::vector<Node> *buf) {
        if (!buf) {
          return;
        }
        for (Node &n : *buf) {
          std::visit(overloaded{[&](AndNode &arg) {
                                  out << NodeTypeID::And << arg.id;
                                  for (unsigned i = 0; i < arg.children.size();
                                       i++) {
                                    out << NodeTypeID::ArcAnd << arg.id
                                        << arg.children[i] << std::endl;
                                  }
                                },
                                [&](OrNode &arg) {
                                  out << NodeTypeID::Or << arg.id;
                                  for (int i = 0; i < 2; i++) {
                                    write_arc(arg.b[i], arg.id);
                                  }
                                },
                                [&](UnaryNode &arg) {
                                  out << NodeTypeID::Or << arg.id;
                                  write_arc(arg.b, arg.id);
                                }},
                     n);
        }
        std::cout << "Writen chunck" << std::endl;
        delete buf;
      };
      while (!end.load()) {
        std::vector<Node> *buf = to_write.exchange(nullptr);
        if (!buf) {
          std::this_thread::sleep_for(10ms);
          continue;
        }
        try_write(buf);
      }
      try_write(to_write.load());
      boost::iostreams::close(outbuf); // Don't forget this!
      file.close();
      std::cout << "Done writer" << std::endl;
    });

  } // constructor.

  /**
     Compute the sum of the given elements.

     @param[in] elts, the elements we want to get the product.
     @param[in] size, the number of elements.

     \return the product of each element of elts.
  */
  size_t manageDeterministOr(DataBranch<size_t> *elts, unsigned size) final {
    assert(size == 2);
    size_t id = index++;
    OrNode data{id, {}};
    out.emplace_back();
    for (int i = 0; i < size; i++) {
      data.b[i] = std::move(elts[i]);
    }
    if (out.size() > 1000000 && to_write.load() == nullptr) {
      std::vector<Node> *tmp = new std::vector<Node>(std::move(out));
      to_write.exchange(tmp);
      std::cout << "Swap Buffers" << std::endl;
    }
    out.push_back(std::move(data));
    return id;
  } // manageDeterministOr

  /**
     Compute the product of the given elements.

     @param[in] elts, the elements we want to get the product.
     @param[in] size, the number of elements.

     \return the product of each element of elts.
   */
  size_t manageDecomposableAnd(size_t *elts, unsigned size)  final {
    size_t id = index++;
    AndNode node{id, std::vector<size_t>(size)};
    for (unsigned i = 0; i < size; i++) {
      node.children[i] = std::move(elts[i]);
    }
    out.push_back(std::move(node));
    return id;
  } // manageDecomposableAnd

  /**
     Manage the case where the problem is unsatisfiable.

     \return 0 as number of models.
   */
  size_t manageBottom() final { return bottom; } // manageBottom

  /**
     Return false, that is given by the value 1.

     \return T(0).
   */
  inline size_t createBottom() { return bottom; }

  /**
     Manage the case where the problem is a tautology.

     @param[in] component, the current set of variables (useless here).

     \return 0 as number of models.
   */
  inline size_t manageTop(std::vector<Var> &component)final { return top; }

  /**
     Return true, that is given by the value 1.

     \return T(1).
   */
  inline size_t createTop() final { return top; }

  /**
     Manage the case where we only have a branch in our OR gate.

     @param[in] e, the branch we are considering.

     \return the number of models associate to the given branch.
   */
  size_t manageBranch(DataBranch<size_t> &e) final {
    size_t node = index++;
    out.push_back(UnaryNode{node, std::move(e)});
    return node;

  } // manageBranch

  /**
     Manage the final result compute.

     @param[in] result, the result we are considering.
     @param[in] config, the configuration that describes what we want to do on the
     given result.
     @param[in] out, the output stream.
   */
  void manageResult(size_t &result, Config &config, std::ostream &out) {
    out.flush();
  } // manageResult

  /**
     Count the number of model, for this case that means doing noting.

     \return the number of models.
   */
  T count(size_t &result) { return 0; } // count

  /**
     Cannot be called, then throws an exception!
   */
  T count(size_t &result, std::vector<Lit> &assum) {
    throw(BadBehaviourException(
        "This operation is not allowed in this context.", __FILE__, __LINE__));
  } // count
};

template <class T> class CountingOperation : public Operation<T, T> {
private:
  ProblemManager *m_problem;

  const T top = T(1);
  const T bottom = T(0);

public:
  CountingOperation() = delete;

  /**
     Constructor.

     @param[in] problem, allows to get information about the problem such as
     weights.
   */
  CountingOperation(ProblemManager *problem)
      : m_problem(problem) {} // constructor.

  /**
     Compute the sum of the given elements.

     @param[in] elts, the elements we want to get the product.
     @param[in] size, the number of elements.

     \return the product of each element of elts.
  */
  T manageDeterministOr(DataBranch<T> *elts, unsigned size) {
    assert(size == 2);
    return elts[0].d * m_problem->computeWeightUnitFree<T>(elts[0].unitLits,
                                                           elts[0].freeVars) +
           elts[1].d * m_problem->computeWeightUnitFree<T>(elts[1].unitLits,
                                                           elts[1].freeVars);
  } // manageDeterministOr

  /**
     Compute the product of the given elements.

     @param[in] elts, the elements we want to get the product.
     @param[in] size, the number of elements.

     \return the product of each element of elts.
   */
  T manageDecomposableAnd(T *elts, unsigned size) {
    if (size == 1)
      return elts[0];
    if (size == 2)
      return elts[0] * elts[1];

    T ret = 1;
    for (unsigned i = 0; i < size; i++)
      ret = ret * elts[i];
    return ret;
  } // manageDecomposableAnd

  /**
     Manage the case where the problem is unsatisfiable.

     \return 0 as number of models.
   */
  T manageBottom() { return bottom; } // manageBottom

  /**
     Return false, that is given by the value 1.

     \return T(0).
   */
  inline T createBottom() { return bottom; }

  /**
     Manage the case where the problem is a tautology.

     @param[in] component, the current set of variables (useless here).

     \return 0 as number of models.
   */
  inline T manageTop(std::vector<Var> &component) { return top; }

  /**
     Return true, that is given by the value 1.

     \return T(1).
   */
  inline T createTop() { return top; }

  /**
     Manage the case where we only have a branch in our OR gate.

     @param[in] e, the branch we are considering.

     \return the number of models associate to the given branch.
   */
  T manageBranch(DataBranch<T> &e) {
    return e.d * m_problem->computeWeightUnitFree<T>(e.unitLits, e.freeVars);
  } // manageBranch

  /**
     Manage the final result compute.

     @param[in] result, the result we are considering.
     @param[in] config, the configuration that describes what we want to do on the
     given result.
     @param[in] out, the output stream.
   */
  void manageResult(T &result, Config &config, std::ostream &out) {
    std::string format = config.keyword_output_format_solution;
    std::string outFormat = config.output_format;

    if (outFormat == "competition") {
      boost::multiprecision::mpf_float::default_precision(128);
      out.precision(std::numeric_limits<
                    boost::multiprecision::cpp_dec_float_50>::digits10);

      if (result == 0) {
        out << "s UNSATISFIABLE\n";
        out << "c " << format << "\n";
        out << "c s log10-estimate -inf\n";
        out << "c s exact quadruple int 0\n";
      } else {
        out << "s SATISFIABLE\n";
        out << "c " << format << "\n";
        out << "c s log10-estimate "
            << boost::multiprecision::log10(
                   boost::multiprecision::cpp_dec_float_100(result))
            << "\n";
        if (config.isFloat)
          out << "c s exact quadruple int " << result << "\n";
        else
          out << "c s exact arb int " << result << "\n";
      }
    } else {
      assert(outFormat == "classic");
      out << format << " ";
      out << std::fixed << std::setprecision(50) << result << "\n";
    }
  } // manageResult

  /**
     Count the number of model, for this case that means doing noting.

     \return the number of models.
   */
  T count(T &result) { return result; } // count

  /**
     Cannot be called, then throws an exception!
   */
  T count(T &result, std::vector<Lit> &assum) {
    throw(BadBehaviourException(
        "This operation is not allowed in this context.", __FILE__, __LINE__));
  } // count
};

} // namespace d4
