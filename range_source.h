/*
 * range_source.h
 *
 *  Created on: Jan 21, 2009
 *      Author: langmead
 */

#ifndef RANGE_SOURCE_H_
#define RANGE_SOURCE_H_

#include <stdint.h>
#include <vector>
#include "seqan/sequence.h"
#include "ebwt.h"
#include "range.h"

/**
 * Encapsulates an algorithm that navigates the Bowtie index to produce
 * candidate ranges of alignments in the Burrows-Wheeler matrix.  A
 * higher authority is responsible for reporting hits out of those
 * ranges, and stopping when the consumer is satisfied.
 */
template<typename TContinuationManager>
class RangeSource {
public:
	RangeSource()  { }
	virtual ~RangeSource() { }

	/// Set query to find ranges for
	virtual void setQuery(seqan::String<Dna5>* qry,
	                      seqan::String<char>* qual,
	                      seqan::String<char>* name) = 0;
	/// Set up the range search.
	virtual bool initConts(TContinuationManager& conts, uint32_t ham) = 0;
	/// Advance the range search by one memory op
	virtual bool advance(TContinuationManager& conts) = 0;
	/// Returns true iff the last call to advance yielded a range
	virtual bool foundRange() = 0;
	/// Return the last valid range found
	virtual Range& range() = 0;
	/// All searching w/r/t the current query is finished
	virtual bool done() = 0;
};

template<typename TCont>
class ContinuationManager {
public:
	virtual ~ContinuationManager() { }
	virtual TCont& front() = 0;
	virtual TCont& back() = 0;
	virtual void   prep(const EbwtParams& ep, const uint8_t* ebwt) = 0;
	virtual void   pop() = 0;
	virtual void   expand1() = 0;
	virtual size_t size() const = 0;
	virtual bool   empty() const = 0;
	virtual void   clear() = 0;
};

/**
 * An aligner for finding exact matches of unpaired reads.  Always
 * tries the forward-oriented version of the read before the reverse-
 * oriented read.
 */
template<typename TRangeSource, typename TContMan>
class RangeSourceDriver {
public:
	RangeSourceDriver(
		const Ebwt<String<Dna> >& ebwt,
		EbwtSearchParams<String<Dna> >& params,
		TRangeSource& rs,
		TContMan& cm,
		bool fw,
		HitSink& sink,
		HitSinkPerThread* sinkPt,
		vector<String<Dna5> >& os,
		bool verbose,
		uint32_t seed) :
		done_(true), first_(true), len_(0),
		pat_(NULL), qual_(NULL), name_(NULL),
		sinkPt_(sinkPt), params_(params),
		fw_(fw), ebwt_(ebwt), rs_(rs), cm_(cm)
	{
		assert(cm_.empty());
	}

	/**
	 * Prepare this aligner for the next read.
	 */
	void setQuery(PatternSourcePerThread* patsrc, bool mate1 = true) {
		if(mate1) {
			if(fw_) {
				pat_  = &patsrc->bufa().patFw;
				qual_ = &patsrc->bufa().qualFw;
			} else {
				pat_  = &patsrc->bufa().patRc;
				qual_ = &patsrc->bufa().qualRc;
			}
			name_ = &patsrc->bufa().name;
		} else {
			if(fw_) {
				pat_  = &patsrc->bufb().patFw;
				qual_ = &patsrc->bufb().qualFw;
			} else {
				pat_  = &patsrc->bufb().patRc;
				qual_ = &patsrc->bufb().qualRc;
			}
			name_ = &patsrc->bufb().name;
		}
		assert(pat_ != NULL);
		assert(qual_ != NULL);
		assert(name_ != NULL);
		len_ = seqan::length(*pat_);
		assert_gt(len_, 0);
		done_ = false;
		first_ = true;
		cm_.clear();
	}

	/**
	 * Advance the aligner by one memory op.  Return true iff we're
	 * done with this read.
	 */
	bool advance() {
		assert(!done_);
		assert(pat_ != NULL);
		params_.setFw(fw_);
		if(first_) {
			// Set up the RangeSource for the forward read
			rs_.setOffs(0, 0, len_, len_, len_, len_);
			rs_.setQuery(pat_, qual_, name_);
			rs_.initConts(cm_, 0); // set up initial continuation
			first_ = false;
		} else {
			// Advance the RangeSource for the forward-oriented read
			rs_.advance(cm_);
		}
		if(!done_) {
			// Finished
			done_ = cm_.empty();
		}
		if(!done_) {
			// Hopefully, this will prefetch enough so that when we
			// resume, stuff is already in cache
			cm_.prep(ebwt_.eh(), ebwt_.ebwt());
		}
		return done_;
	}

	/**
	 * Return true iff we just found a range.
	 */
	bool foundRange() {
		return rs_.foundRange();
	}

	/**
	 * Return the range found.
	 */
	Range& range() {
		return rs_.range();
	}

	/**
	 * Returns true if all searching w/r/t the current query is
	 * finished or if there is no current query.
	 */
	bool done() {
		return done_;
	}

	/// Return length of current query
	uint32_t qlen() {
		return len_;
	}

protected:
	// Progress state
	bool done_;
	bool first_;
	uint32_t len_;
	String<Dna5>* pat_;
	String<char>* qual_;
	String<char>* name_;

	// Temporary HitSink; to be deleted
	HitSinkPerThread* sinkPt_;

	// State for alignment
	EbwtSearchParams<String<Dna> >& params_;
	bool                            fw_;
	const Ebwt<String<Dna> >&       ebwt_;
	TRangeSource&                   rs_;
	TContMan&                       cm_;
};
#endif /* RANGE_SOURCE_H_ */
