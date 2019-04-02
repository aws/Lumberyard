/*
 * All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
 * its licensors.
 *
 * For complete copyright and license terms please see the LICENSE at the root of this
 * distribution (the "License"). All use of this software is governed by the License,
 * or, if provided, by the license below or the license accompanying this file. Do not
 * remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 */
#pragma once

#include "AzCore/Outcome/Internal/OutcomeStorage.h"

namespace AZ
{
    //////////////////////////////////////////////////////////////////////////
    // Success Implementation

    inline SuccessValue<void> Success()
    {
        return SuccessValue<void>();
    }

#ifdef AZ_HAS_RVALUE_REFS
    template <class ValueT, class>
    inline SuccessValue<ValueT> Success(ValueT&& rhs)
    {
        return SuccessValue<ValueT>(AZStd::forward<ValueT>(rhs));
    }
#else // AZ_HAS_RVALUE_REFS
    template <class ValueT, class>
    inline SuccessValue<ValueT> Success(const ValueT& rhs)
    {
        return SuccessValue<ValueT>(rhs);
    }
#endif // AZ_HAS_RVALUE_REFS

    //////////////////////////////////////////////////////////////////////////
    // Failure Implementation

    inline FailureValue<void> Failure()
    {
        return FailureValue<void>();
    }

#ifdef AZ_HAS_RVALUE_REFS
    template <class ValueT, class>
    inline FailureValue<ValueT> Failure(ValueT&& rhs)
    {
        return FailureValue<ValueT>(AZStd::forward<ValueT>(rhs));
    }
#else // AZ_HAS_RVALUE_REFS
    template <class ValueT, class>
    inline FailureValue<ValueT> Failure(const ValueT& rhs)
    {
        return FailureValue<ValueT>(AZStd::forward<ValueT>(rhs));
    }
#endif // AZ_HAS_RVALUE_REFS

    template<typename ErrorT>
    struct DefaultFailure
    {
        static FailureValue<ErrorT> Construct()
        {
            return AZ::Failure(ErrorT{});
        }
    };

    template<>
    struct DefaultFailure<void>
    {
        static FailureValue<void> Construct()
        {
            return AZ::Failure();
        }
    };

    ////////////////////////////////////////////////////////////////////////////
    // Outcome Implementation
    template <class ValueT, class ErrorT>
    AZ_FORCE_INLINE Outcome<ValueT, ErrorT>::Outcome()
        : m_isSuccess(false)
    {
        ConstructFailure(AZ::DefaultFailure<ErrorT>::Construct());
    }

    template <class ValueT, class ErrorT>
    AZ_FORCE_INLINE Outcome<ValueT, ErrorT>::Outcome(const SuccessType& success)
        : m_isSuccess(true)
    {
        ConstructSuccess(success);
    }

#ifdef AZ_HAS_RVALUE_REFS
    template <class ValueT, class ErrorT>
    AZ_FORCE_INLINE Outcome<ValueT, ErrorT>::Outcome(SuccessType&& success)
        : m_isSuccess(true)
    {
        ConstructSuccess(AZStd::move(success));
    }
#endif // AZ_HAS_RVALUE_REFS

    template <class ValueT, class ErrorT>
    AZ_FORCE_INLINE Outcome<ValueT, ErrorT>::Outcome(const FailureType& failure)
        : m_isSuccess(false)
    {
        ConstructFailure(failure);
    }

#ifdef AZ_HAS_RVALUE_REFS
    template <class ValueT, class ErrorT>
    AZ_FORCE_INLINE Outcome<ValueT, ErrorT>::Outcome(FailureType&& failure)
        : m_isSuccess(false)
    {
        ConstructFailure(AZStd::move(failure));
    }
#endif // AZ_HAS_RVALUE_REFS

    template <class ValueT, class ErrorT>
    AZ_FORCE_INLINE Outcome<ValueT, ErrorT>::Outcome(const Outcome& other)
        : m_isSuccess(other.m_isSuccess)
    {
        if (m_isSuccess)
        {
            ConstructSuccess(other.GetSuccess());
        }
        else
        {
            ConstructFailure(other.GetFailure());
        }
    }

#ifdef AZ_HAS_RVALUE_REFS
    template <class ValueT, class ErrorT>
    AZ_FORCE_INLINE Outcome<ValueT, ErrorT>::Outcome(Outcome&& other)
        : m_isSuccess(other.m_isSuccess)
    {
        if (m_isSuccess)
        {
            ConstructSuccess(AZStd::move(other.GetSuccess()));
        }
        else
        {
            ConstructFailure(AZStd::move(other.GetFailure()));
        }
    }
#endif // AZ_HAS_RVALUE_REFS

    template <class ValueT, class ErrorT>
    AZ_FORCE_INLINE Outcome<ValueT, ErrorT>::~Outcome()
    {
        if (m_isSuccess)
        {
            GetSuccess().~SuccessType();
        }
        else
        {
            GetFailure().~FailureType();
        }
    }

    template <class ValueT, class ErrorT>
    AZ_FORCE_INLINE Outcome<ValueT, ErrorT>& Outcome<ValueT, ErrorT>::operator=(const Outcome& other)
    {
        if (other.m_isSuccess)
        {
            return this->operator=(other.GetSuccess());
        }
        else
        {
            return this->operator=(other.GetFailure());
        }
    }

    template <class ValueT, class ErrorT>
    AZ_FORCE_INLINE Outcome<ValueT, ErrorT>& Outcome<ValueT, ErrorT>::operator=(const SuccessType& success)
    {
        if (m_isSuccess)
        {
            GetSuccess() = success;
        }
        else
        {
            GetFailure().~FailureType();
            m_isSuccess = true;
            ConstructSuccess(success);
        }
        return *this;
    }

    template <class ValueT, class ErrorT>
    AZ_FORCE_INLINE Outcome<ValueT, ErrorT>& Outcome<ValueT, ErrorT>::operator=(const FailureType& failure)
    {
        if (!m_isSuccess)
        {
            GetFailure() = failure;
        }
        else
        {
            GetSuccess().~SuccessType();
            m_isSuccess = false;
            ConstructFailure(failure);
        }
        return *this;
    }

#ifdef AZ_HAS_RVALUE_REFS
    template <class ValueT, class ErrorT>
    AZ_FORCE_INLINE Outcome<ValueT, ErrorT>& Outcome<ValueT, ErrorT>::operator=(Outcome&& other)
    {
        if (other.m_isSuccess)
        {
            return this->operator=(AZStd::move(other.GetSuccess()));
        }
        else
        {
            return this->operator=(AZStd::move(other.GetFailure()));
        }
    }

    template <class ValueT, class ErrorT>
    AZ_FORCE_INLINE Outcome<ValueT, ErrorT>& Outcome<ValueT, ErrorT>::operator=(SuccessType&& success)
    {
        if (m_isSuccess)
        {
            GetSuccess() = AZStd::move(success);
        }
        else
        {
            GetFailure().~FailureType();
            m_isSuccess = true;
            ConstructSuccess(AZStd::move(success));
        }
        return *this;
    }

    template <class ValueT, class ErrorT>
    AZ_FORCE_INLINE Outcome<ValueT, ErrorT>& Outcome<ValueT, ErrorT>::operator=(FailureType&& failure)
    {
        if (!m_isSuccess)
        {
            GetFailure() = AZStd::move(failure);
        }
        else
        {
            GetSuccess().~SuccessType();
            m_isSuccess = false;
            ConstructFailure(AZStd::move(failure));
        }
        return *this;
    }
#endif // AZ_HAS_RVALUE_REFS

    template <class ValueT, class ErrorT>
    AZ_FORCE_INLINE bool Outcome<ValueT, ErrorT>::IsSuccess() const
    {
        return m_isSuccess;
    }

    template <class ValueT, class ErrorT>
    AZ_FORCE_INLINE Outcome<ValueT, ErrorT>::operator bool() const
    {
        return IsSuccess();
    }

    template <class ValueT, class ErrorT>
    template <class Value_Type, class>
    AZ_FORCE_INLINE Value_Type& Outcome<ValueT, ErrorT>::GetValue()
    {
        return GetSuccess().m_value;
    }

    template <class ValueT, class ErrorT>
    template <class Value_Type, class>
    AZ_FORCE_INLINE const Value_Type& Outcome<ValueT, ErrorT>::GetValue() const
    {
        return GetSuccess().m_value;
    }

#ifdef AZ_HAS_RVALUE_REFS
    template <class ValueT, class ErrorT>
    template <class Value_Type, class>
    AZ_FORCE_INLINE Value_Type && Outcome<ValueT, ErrorT>::TakeValue()
    {
        return AZStd::move(GetSuccess().m_value);
    }
#endif // AZ_HAS_RVALUE_REFS

#ifdef AZ_HAS_RVALUE_REFS
    template <class ValueT, class ErrorT>
    template <class U, class Value_Type, class>
    AZ_FORCE_INLINE Value_Type Outcome<ValueT, ErrorT>::GetValueOr(U&& defaultValue) const
    {
        return m_isSuccess
            ? GetValue()
            : static_cast<U>(AZStd::forward<U>(defaultValue));
    }
#else
    template <class ValueT, class ErrorT>
    template <class Value_Type, class>
    AZ_FORCE_INLINE Value_Type Outcome<ValueT, ErrorT>::GetValueOr(const ValueType& defaultValue) const
    {
        return m_isSuccess
            ? GetValue()
            : defaultValue;
    }
#endif // AZ_HAS_RVALUE_REFS

    template <class ValueT, class ErrorT>
    template <class Error_Type, class>
    AZ_FORCE_INLINE Error_Type& Outcome<ValueT, ErrorT>::GetError()
    {
        return GetFailure().m_value;
    }

    template <class ValueT, class ErrorT>
    template <class Error_Type, class>
    AZ_FORCE_INLINE const Error_Type& Outcome<ValueT, ErrorT>::GetError() const
    {
        return GetFailure().m_value;
    }

#ifdef AZ_HAS_RVALUE_REFS
    template <class ValueT, class ErrorT>
    template <class Error_Type, class>
    AZ_FORCE_INLINE Error_Type && Outcome<ValueT, ErrorT>::TakeError()
    {
        return AZStd::move(GetFailure().m_value);
    }
#endif // AZ_HAS_RVALUE_REFS

    template <class ValueT, class ErrorT>
    typename Outcome<ValueT, ErrorT>::SuccessType & Outcome<ValueT, ErrorT>::GetSuccess()
    {
        AZ_Assert(m_isSuccess, "AZ::Outcome was a failure, no success value exists, returning garbage!");
        return reinterpret_cast<SuccessType&>(m_success);
    }

    template <class ValueT, class ErrorT>
    const typename Outcome<ValueT, ErrorT>::SuccessType & Outcome<ValueT, ErrorT>::GetSuccess() const
    {
        AZ_Assert(m_isSuccess, "AZ::Outcome was a failure, no success value exists, returning garbage!");
        return reinterpret_cast<const SuccessType&>(m_success);
    }

    template <class ValueT, class ErrorT>
    typename Outcome<ValueT, ErrorT>::FailureType & Outcome<ValueT, ErrorT>::GetFailure()
    {
        AZ_Assert(!m_isSuccess, "AZ::Outcome was a success, no error exists, returning garbage!");
        return reinterpret_cast<FailureType&>(m_failure);
    }

    template <class ValueT, class ErrorT>
    const typename Outcome<ValueT, ErrorT>::FailureType & Outcome<ValueT, ErrorT>::GetFailure() const
    {
        AZ_Assert(!m_isSuccess, "AZ::Outcome was a success, no error exists, returning garbage!");
        return reinterpret_cast<const FailureType&>(m_failure);
    }

    template <class ValueT, class ErrorT>
    template<class ... ArgsT>
    void Outcome<ValueT, ErrorT>::ConstructSuccess(ArgsT&& ... args)
    {
        AZ_Assert(m_isSuccess, "AZ::Outcome::ConstructSuccess(...) - Cannot construct success in failed outcome.");
        new(AZStd::addressof(m_success))SuccessType(AZStd::forward<ArgsT>(args) ...);
    }

    template <class ValueT, class ErrorT>
    template<class ... ArgsT>
    void Outcome<ValueT, ErrorT>::ConstructFailure(ArgsT&& ... args)
    {
        AZ_Assert(!m_isSuccess, "AZ::Outcome::ConstructFailure(...) - Cannot construct failure in successful outcome.");
        new(AZStd::addressof(m_failure))FailureType(AZStd::forward<ArgsT>(args) ...);
    }
} // namespace AZ
