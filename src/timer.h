// CaboCha -- Yet Another Japanese Dependency Parser
//
//  $Id: timer.h 41 2008-01-20 09:31:34Z taku-ku $;
//
//  Copyright(C) 2001-2008 Taku Kudo <taku@chasen.org>
#ifndef CABOCHA_TIMER_H_
#define CABOCHA_TIMER_H_

#include <ctime>
#include <iostream>
#include <string>
#include <limits>

#undef max
#undef min

namespace CaboCha {

  class timer {
  public:
    explicit timer() { start_time_ = std::clock(); }
    void   restart() { start_time_ = std::clock(); }
    double elapsed() const {
      return  double(std::clock() - start_time_) / CLOCKS_PER_SEC; }

    double elapsed_max() const {
      return (double(std::numeric_limits<std::clock_t>::max())
              - double(start_time_)) / double(CLOCKS_PER_SEC);
    }

    double elapsed_min() const {
      return double(1)/double(CLOCKS_PER_SEC);
    }

  private:
    std::clock_t start_time_;
  };

  class progress_timer : public timer {

  public:
    explicit progress_timer( std::ostream & os = std::cout ) : os_(os) {}
    ~progress_timer()
      {
        std::istream::fmtflags old_flags = os_.setf( std::istream::fixed,
                                                     std::istream::floatfield );
        std::streamsize old_prec = os_.precision( 2 );
        os_ << elapsed() << " s\n" << std::endl;
        os_.flags( old_flags );
        os_.precision( old_prec );
      }

  private:
    std::ostream & os_;
  };
}

#endif
