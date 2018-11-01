// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

class CTimeValue
{
public:
	static const signed long long TIMEVALUE_PRECISION = 100000; //!< One second.

public:
	void GetMemoryUsage(class ICrySizer* pSizer) const { /*nothing*/ }

	inline CTimeValue()
	{
		m_lValue = 0;
	}

	inline CTimeValue(const float fSeconds)
	{
		SetSeconds(fSeconds);
	}

	inline CTimeValue(const double fSeconds)
	{
		SetSeconds(fSeconds);
	}

	//! \param inllValue Positive negative, absolute or relative in 1 second= TIMEVALUE_PRECISION units.
	inline CTimeValue(const signed long long inllValue)
	{
		m_lValue = inllValue;
	}

	//! Copy constructor.
	inline CTimeValue(const CTimeValue& inValue)
	{
		m_lValue = inValue.m_lValue;
	}

	inline ~CTimeValue() {}

	//! Assignment operator.
	//! \param inRhs Right side.
	inline CTimeValue& operator=(const CTimeValue& inRhs)
	{
		m_lValue = inRhs.m_lValue;
		return *this;
	};

	//! Use only for relative value, absolute values suffer a lot from precision loss.
	inline float GetSeconds() const
	{
		return m_lValue * (1.f / TIMEVALUE_PRECISION);
	}

	//! Get relative time difference in seconds.
	//! Call this on the endTime object: endTime.GetDifferenceInSeconds( startTime );
	inline float GetDifferenceInSeconds(const CTimeValue& startTime) const
	{
		return (m_lValue - startTime.m_lValue) * (1.f / TIMEVALUE_PRECISION);
	}

	inline void SetSeconds(const float infSec)
	{
		m_lValue = (signed long long)(infSec * TIMEVALUE_PRECISION);
	}

	inline void SetSeconds(const double infSec)
	{
		m_lValue = (signed long long)(infSec * TIMEVALUE_PRECISION);
	}

	inline void SetSeconds(const signed long long indwSec)
	{
		m_lValue = indwSec * TIMEVALUE_PRECISION;
	}

	inline void SetMilliSeconds(const signed long long indwMilliSec)
	{
		m_lValue = indwMilliSec * (TIMEVALUE_PRECISION / 1000);
	}

	//! Use only for relative value, absolute values suffer a lot from precision loss.
	inline float GetMilliSeconds() const
	{
		return m_lValue * (1000.f / TIMEVALUE_PRECISION);
	}

	inline signed long long GetMilliSecondsAsInt64() const
	{
		return m_lValue * 1000 / TIMEVALUE_PRECISION;
	}

	inline signed long long GetMicroSecondsAsInt64() const
	{
		return m_lValue * (1000 * 1000) / TIMEVALUE_PRECISION;
	}

	inline signed long long GetValue() const
	{
		return m_lValue;
	}

	inline void SetValue(signed long long val)
	{
		m_lValue = val;
	}

	//! Useful for periodic events (e.g. water wave, blinking).
	//! Changing TimePeriod can results in heavy changes in the returned value.
	//! \return [0..1]
	float GetPeriodicFraction(const CTimeValue TimePeriod) const
	{
		// todo: change float implement to int64 for more precision
		float fAbs = GetSeconds() / TimePeriod.GetSeconds();
		return fAbs - (int)(fAbs);
	}

	// math operations

	//! Binary subtraction.
	inline CTimeValue operator-(const CTimeValue& inRhs) const { CTimeValue ret; ret.m_lValue = m_lValue - inRhs.m_lValue; return ret; };

	//! Binary addition.
	inline CTimeValue operator+(const CTimeValue& inRhs) const { CTimeValue ret; ret.m_lValue = m_lValue + inRhs.m_lValue; return ret;  };

	//! Sign inversion.
	inline CTimeValue  operator-() const                   { CTimeValue ret; ret.m_lValue = -m_lValue; return ret; };

	inline CTimeValue& operator+=(const CTimeValue& inRhs) { m_lValue += inRhs.m_lValue; return *this; }
	inline CTimeValue& operator-=(const CTimeValue& inRhs) { m_lValue -= inRhs.m_lValue; return *this; }

	inline CTimeValue& operator/=(int inRhs)               { m_lValue /= inRhs; return *this; }

	// comparison -----------------------

	inline bool operator<(const CTimeValue& inRhs) const  { return m_lValue < inRhs.m_lValue; };
	inline bool operator>(const CTimeValue& inRhs) const  { return m_lValue > inRhs.m_lValue; };
	inline bool operator>=(const CTimeValue& inRhs) const { return m_lValue >= inRhs.m_lValue; };
	inline bool operator<=(const CTimeValue& inRhs) const { return m_lValue <= inRhs.m_lValue; };
	inline bool operator==(const CTimeValue& inRhs) const { return m_lValue == inRhs.m_lValue; };
	inline bool operator!=(const CTimeValue& inRhs) const { return m_lValue != inRhs.m_lValue; };

	

	void GetMemoryStatistics(class ICrySizer* pSizer) const { /*nothing*/ }

private:
	signed long long m_lValue;     //!< Absolute or relative value in 1/TIMEVALUE_PRECISION, might be negative.

	friend class CTimer;
};
