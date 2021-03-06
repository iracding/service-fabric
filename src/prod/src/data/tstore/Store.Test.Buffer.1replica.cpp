// ------------------------------------------------------------
// Copyright (c) Microsoft Corporation.  All rights reserved.
// Licensed under the MIT License (MIT). See License.txt in the repo root for license information.
// ------------------------------------------------------------

#include "stdafx.h"

#include <boost/test/unit_test.hpp>
#include "Common/boost-taef.h"
#include "TStoreTestBase.h"

#define ALLOC_TAG 'tsTP'

namespace TStoreTests
{
    using namespace ktl;

    class StoreTestBuffer1Replica : public TStoreTestBase<KBuffer::SPtr, KBuffer::SPtr, KBufferComparer, KBufferSerializer, KBufferSerializer>
    {
    public:
        StoreTestBuffer1Replica()
        {
            Setup();
        }

        ~StoreTestBuffer1Replica()
        {
            Cleanup();
        }

        KBuffer::SPtr ToBuffer(__in ULONG num)
        {
            KBuffer::SPtr bufferSptr;
            auto status = KBuffer::Create(sizeof(ULONG), bufferSptr, GetAllocator());
            CODING_ERROR_ASSERT(NT_SUCCESS(status));

            auto buffer = (ULONG *)bufferSptr->GetBuffer();
            buffer[0] = num;
            return bufferSptr;
        }

        static bool EqualityFunction(KBuffer::SPtr & one, KBuffer::SPtr & two)
        {
            return *one == *two;
        }

        Common::CommonConfig config; // load the config object as its needed for the tracing to work

#pragma region test functions
    public:
        ktl::Awaitable<void> Add_SingleTransaction_ShouldSucceed_Test()
        {
            KBuffer::SPtr key = ToBuffer(5);
            KBuffer::SPtr value = ToBuffer(6);

            {
                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
                co_await Store->AddAsync(*tx->StoreTransactionSPtr, key, value, Common::TimeSpan::MaxValue, CancellationToken::None);
                co_await VerifyKeyExistsAsync(*Store, *tx->StoreTransactionSPtr, key, nullptr, value, EqualityFunction);
                co_await tx->CommitAsync();
            }

            co_await VerifyKeyExistsAsync(*Store, key, nullptr, value, EqualityFunction);
            co_return;
        }

        ktl::Awaitable<void> AddUpdate_SingleTransaction_ShouldSucceed_Test()
        {
            KBuffer::SPtr key = ToBuffer(5);
            KBuffer::SPtr value = ToBuffer(6);
            KBuffer::SPtr updatedValue = ToBuffer(7);

            {
                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
                co_await Store->AddAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None);
                co_await Store->ConditionalUpdateAsync(*tx->StoreTransactionSPtr, key, updatedValue, DefaultTimeout, CancellationToken::None);
                co_await VerifyKeyExistsAsync(*Store, *tx->StoreTransactionSPtr, key, nullptr, updatedValue, EqualityFunction);
                co_await tx->CommitAsync();
            }

            co_await VerifyKeyExistsAsync(*Store, key, nullptr, updatedValue, EqualityFunction);
            co_return;
        }

        ktl::Awaitable<void> AddDelete_SingleTransaction_ShouldSucceed_Test()
        {
            KBuffer::SPtr key = ToBuffer(5);
            KBuffer::SPtr value = ToBuffer(6);

            {
                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
                co_await Store->AddAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None);
                co_await Store->ConditionalRemoveAsync(*tx->StoreTransactionSPtr, key, DefaultTimeout, CancellationToken::None);
                co_await tx->CommitAsync();
            }

            co_await VerifyKeyDoesNotExistAsync(*Store, key);
            co_return;
        }

        ktl::Awaitable<void> AddDeleteAdd_SingleTransaction_ShouldSucceed_Test()
        {
            KBuffer::SPtr key = ToBuffer(5);
            KBuffer::SPtr value = ToBuffer(6);

            {
                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
                co_await Store->AddAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None);
                co_await Store->ConditionalRemoveAsync(*tx->StoreTransactionSPtr, key, DefaultTimeout, CancellationToken::None);
                co_await Store->AddAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None);
                co_await VerifyKeyExistsAsync(*Store, *tx->StoreTransactionSPtr, key, nullptr, value, EqualityFunction);
                co_await tx->CommitAsync();
            }

            co_await VerifyKeyExistsAsync(*Store, key, nullptr, value, EqualityFunction);
            co_return;
        }

        ktl::Awaitable<void> NoAdd_Delete_ShouldFail_Test()
        {
            KBuffer::SPtr key = ToBuffer(5);

            {
                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
                co_await Store->ConditionalRemoveAsync(*tx->StoreTransactionSPtr, key, DefaultTimeout, CancellationToken::None);
                co_await tx->AbortAsync();
            }

            co_await VerifyKeyDoesNotExistAsync(*Store, key);
            co_return;
        }

        ktl::Awaitable<void> AddDeleteUpdate_SingleTransaction_ShouldFail_Test()
        {
            KBuffer::SPtr key = ToBuffer(5);
            KBuffer::SPtr value = ToBuffer(6);
            KBuffer::SPtr updateValue = ToBuffer(7);

            {
                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
                co_await Store->AddAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None);
                bool result_remove = co_await Store->ConditionalRemoveAsync(*tx->StoreTransactionSPtr, key, DefaultTimeout, CancellationToken::None);
                VERIFY_ARE_EQUAL(result_remove, true);
                co_await VerifyKeyDoesNotExistAsync(*Store, *tx->StoreTransactionSPtr, key);
                bool result_update = co_await Store->ConditionalUpdateAsync(*tx->StoreTransactionSPtr, key, updateValue, DefaultTimeout, CancellationToken::None);
                VERIFY_ARE_EQUAL(result_update, false);
                co_await tx->AbortAsync();
            }

            co_await VerifyKeyDoesNotExistAsync(*Store, key);
            co_return;
        }

        ktl::Awaitable<void> AddAdd_SingleTransaction_ShouldFail_Test()
        {
            bool secondAddFailed = false;

            KBuffer::SPtr key = ToBuffer(5);
            KBuffer::SPtr value = ToBuffer(6);

            WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
            co_await Store->AddAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None);

            try
            {
                co_await Store->AddAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None);
            }
            catch (ktl::Exception &)
            {
                // todo once status codes are fixed, fix this as well
                secondAddFailed = true;
            }

            CODING_ERROR_ASSERT(secondAddFailed == true);
            co_await tx->AbortAsync();
            co_return;
        }

        ktl::Awaitable<void> MultipleAdds_SingleTransaction_ShouldSucceed_Test()
        {
            {
                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();

                for (int i = 0; i < 10; i++)
                {
                    co_await Store->AddAsync(*tx->StoreTransactionSPtr, ToBuffer(i), ToBuffer(i), DefaultTimeout, CancellationToken::None);
                }

                co_await tx->CommitAsync();
            }

            for (int i = 0; i < 10; i++)
            {
                KBuffer::SPtr expectedValue = ToBuffer(i);
                co_await VerifyKeyExistsAsync(*Store, ToBuffer(i), nullptr, expectedValue, EqualityFunction);
            }
            co_return;
        }

        ktl::Awaitable<void> MultipleUpdates_SingleTransaction_ShouldSucceed_Test()
        {
            KBuffer::SPtr key = ToBuffer(5);

            {
                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();

                KBuffer::SPtr value = ToBuffer(6);

                co_await Store->AddAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None);

                for (int i = 0; i < 10; i++)
                {
                    co_await Store->ConditionalUpdateAsync(*tx->StoreTransactionSPtr, key, ToBuffer(i), DefaultTimeout, CancellationToken::None);
                }

                co_await tx->CommitAsync();
            }

            KBuffer::SPtr expectedValue = ToBuffer(9);
            co_await VerifyKeyExistsAsync(*Store, key, nullptr, expectedValue, EqualityFunction);
            co_return;
        }

        ktl::Awaitable<void> AddUpdate_DifferentTransactions_ShouldSucceed_Test()
        {
            KBuffer::SPtr key = ToBuffer(5);
            KBuffer::SPtr value = ToBuffer(5);
            KBuffer::SPtr updateValue = ToBuffer(6);

            {
                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx1 = CreateWriteTransaction();
                co_await Store->AddAsync(*tx1->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None);
                co_await VerifyKeyExistsAsync(*Store, *tx1->StoreTransactionSPtr, key, nullptr, value, EqualityFunction);
                co_await tx1->CommitAsync();
            }

            {
                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx2 = CreateWriteTransaction();
                co_await Store->ConditionalUpdateAsync(*tx2->StoreTransactionSPtr, key, updateValue, DefaultTimeout, CancellationToken::None);
                co_await tx2->CommitAsync();
            }

            co_await VerifyKeyExistsAsync(*Store, key, nullptr, updateValue, EqualityFunction);
            co_return;
        }

        ktl::Awaitable<void> AddDelete_DifferentTransactions_ShouldSucceed_Test()
        {
            KBuffer::SPtr key = ToBuffer(5);

            {
                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx1 = CreateWriteTransaction();
                KBuffer::SPtr value = ToBuffer(5);

                co_await Store->AddAsync(*tx1->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None);
                co_await VerifyKeyExistsAsync(*Store, *tx1->StoreTransactionSPtr, key, nullptr, value, EqualityFunction);
                co_await tx1->CommitAsync();
            }

            {
                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx2 = CreateWriteTransaction();
                bool result = co_await Store->ConditionalRemoveAsync(*tx2->StoreTransactionSPtr, key, DefaultTimeout, CancellationToken::None);
                CODING_ERROR_ASSERT(result == true);
                co_await tx2->CommitAsync();
            }

            co_await VerifyKeyDoesNotExistAsync(*Store, key);
            co_return;
        }

        ktl::Awaitable<void> AddUpdateRead_DifferentTransactions_ShouldSucceed_Test()
        {
            KBuffer::SPtr key = ToBuffer(5);
            KBuffer::SPtr value = ToBuffer(5);
            KBuffer::SPtr updateValue = ToBuffer(7);

            {
                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx1 = CreateWriteTransaction();
                co_await Store->AddAsync(*tx1->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None);
                co_await VerifyKeyExistsAsync(*Store, *tx1->StoreTransactionSPtr, key, nullptr, value, EqualityFunction);
                co_await tx1->CommitAsync();
            }

            {
                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx2 = CreateWriteTransaction();
                bool res = co_await Store->ConditionalUpdateAsync(*tx2->StoreTransactionSPtr, key, updateValue, DefaultTimeout, CancellationToken::None);
                CODING_ERROR_ASSERT(res == true);
                co_await tx2->CommitAsync();
            }

            co_await VerifyKeyExistsAsync(*Store, key, nullptr, updateValue, EqualityFunction);
            co_return;
        }

        ktl::Awaitable<void> AddDeleteUpdate_DifferentTransactions_ShouldFail_Test()
        {
            KBuffer::SPtr key = ToBuffer(5);
            KBuffer::SPtr value = ToBuffer(5);
            KBuffer::SPtr updateValue = ToBuffer(7);

            {
                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx1 = CreateWriteTransaction();
                co_await Store->AddAsync(*tx1->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None);
                co_await VerifyKeyExistsAsync(*Store, *tx1->StoreTransactionSPtr, key, nullptr, value, EqualityFunction);
                co_await tx1->CommitAsync();
            }

            {
                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx2 = CreateWriteTransaction();
                bool result = co_await Store->ConditionalRemoveAsync(*tx2->StoreTransactionSPtr, key, DefaultTimeout, CancellationToken::None);
                CODING_ERROR_ASSERT(result == true);
                co_await tx2->CommitAsync();
            }

            {
                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx3 = CreateWriteTransaction();
                bool res = co_await Store->ConditionalUpdateAsync(*tx3->StoreTransactionSPtr, key, updateValue, DefaultTimeout, CancellationToken::None);
                CODING_ERROR_ASSERT(res == false);
                co_await tx3->AbortAsync();
            }

            co_await VerifyKeyDoesNotExistAsync(*Store, key);
            co_return;
        }

        ktl::Awaitable<void> MultipleAdds_MultipleTransactions_ShouldSucceed_Test()
        {
            for (int i = 0; i < 10; i++)
            {
                KBuffer::SPtr key = ToBuffer(i);
                KBuffer::SPtr value = ToBuffer(i);

                {
                    WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
                    co_await Store->AddAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None);
                    co_await tx->CommitAsync();
                }
            }

            {
                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
                for (int i = 0; i < 10; i++)
                {
                    co_await VerifyKeyExistsAsync(*Store, *tx->StoreTransactionSPtr, ToBuffer(i), nullptr, ToBuffer(i), EqualityFunction);
                }

                co_await tx->AbortAsync();
            }
            co_return;
        }

        ktl::Awaitable<void> MultipleAddsUpdates_MultipleTransactions_ShouldSucceed_Test()
        {
            for (int i = 0; i < 10; i++)
            {
                KBuffer::SPtr key = ToBuffer(i);
                KBuffer::SPtr value = ToBuffer(i);

                {
                    WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
                    co_await Store->AddAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None);
                    co_await tx->CommitAsync();
                }
            }

            for (int i = 0; i < 10; i++)
            {
                KBuffer::SPtr key = ToBuffer(i);
                KBuffer::SPtr value = ToBuffer(i+10);

                {
                    WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
                    co_await Store->ConditionalUpdateAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None);
                    co_await tx->CommitAsync();
                }
            }

            {
                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
                for (int i = 0; i < 10; i++)
                {
                    // Verify updated value
                    co_await VerifyKeyExistsAsync(*Store, *tx->StoreTransactionSPtr, ToBuffer(i), nullptr, ToBuffer(i+10), EqualityFunction);
                }

                co_await tx->AbortAsync();
            }
            co_return;
        }

        ktl::Awaitable<void> Add_Abort_ShouldSucceed_Test()
        {
            KBuffer::SPtr key = ToBuffer(5);
            KBuffer::SPtr value = ToBuffer(5);

            {
                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
                co_await Store->AddAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None);
                KeyValuePair<LONG64, KBuffer::SPtr> kvpair(-1, ToBuffer(0));
                co_await Store->ConditionalGetAsync(*tx->StoreTransactionSPtr, key, DefaultTimeout, kvpair, CancellationToken::None);
                CODING_ERROR_ASSERT(*kvpair.Value == *value);
                CODING_ERROR_ASSERT(kvpair.Key == 0);
                co_await tx->AbortAsync();
            }

            co_await VerifyKeyDoesNotExistAsync(*Store, key);
            co_return;
        }

        ktl::Awaitable<void> AddAdd_SameKeyOnConcurrentTransaction_ShouldTimeout_Test()
        {
            bool hasFailed = false;

            KBuffer::SPtr key = ToBuffer(5);
            KBuffer::SPtr value = ToBuffer(6);

            {
                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx1 = CreateWriteTransaction();
                co_await Store->AddAsync(*tx1->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None);

                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx2 = CreateWriteTransaction();
                try
                {
                    co_await Store->AddAsync(*tx2->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None);
                }
                catch (ktl::Exception & e)
                {
                    hasFailed = true;
                    CODING_ERROR_ASSERT(e.GetStatus() == SF_STATUS_TIMEOUT);
                }

                CODING_ERROR_ASSERT(hasFailed);

                co_await tx2->AbortAsync();
                co_await tx1->AbortAsync();
            }

            co_await VerifyKeyDoesNotExistAsync(*Store, key);
            co_return;
        }

        ktl::Awaitable<void> UpdateRead_SameKeyOnConcurrentTransaction_ShouldTimeout_Test()
        {
            bool hasFailed = false;

            KBuffer::SPtr key = ToBuffer(5);
            KBuffer::SPtr value = ToBuffer(6);

            {
                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
                co_await Store->AddAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None);
                co_await tx->CommitAsync();
            }

            {
                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx1 = CreateWriteTransaction();
                co_await Store->ConditionalUpdateAsync(*tx1->StoreTransactionSPtr, key, ToBuffer(8), DefaultTimeout, CancellationToken::None);

                {
                    WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx2 = CreateWriteTransaction();
                    tx2->StoreTransactionSPtr->ReadIsolationLevel = StoreTransactionReadIsolationLevel::ReadRepeatable;
                    try
                    {
                        co_await VerifyKeyExistsAsync(*Store, *tx2->StoreTransactionSPtr, key, nullptr, ToBuffer(8), EqualityFunction);
                    }
                    catch (ktl::Exception & e)
                    {
                        hasFailed = true;
                        CODING_ERROR_ASSERT(e.GetStatus() == SF_STATUS_TIMEOUT);
                    }

                    CODING_ERROR_ASSERT(hasFailed);

                    co_await tx2->AbortAsync();
                    co_await tx1->AbortAsync();
                }
            }

            co_await VerifyKeyExistsAsync(*Store, key, nullptr, value, EqualityFunction);
            co_return;
        }

        ktl::Awaitable<void> UpdateUpdate_SameKeyOnConcurrentTransaction_WithInfiniteTimeout_ShouldSucceed_Test()
        {
            KBuffer::SPtr key = ToBuffer(5);
            KBuffer::SPtr value = ToBuffer(6);

            {
                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
                co_await Store->AddAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None);
                co_await tx->CommitAsync();
            }

            {
                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx1 = CreateWriteTransaction();
                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx2 = CreateWriteTransaction();

                auto update1Task = Store->ConditionalUpdateAsync(*tx1->StoreTransactionSPtr, key, ToBuffer(8), Common::TimeSpan::FromTicks(-1), CancellationToken::None);
                auto update2Task = Store->ConditionalUpdateAsync(*tx2->StoreTransactionSPtr, key, ToBuffer(8), Common::TimeSpan::FromTicks(-1), CancellationToken::None);

                co_await update1Task;
                CODING_ERROR_ASSERT(update2Task.IsComplete() == false);

                co_await tx1->CommitAsync();
                tx1 = nullptr; // Dispose transaction to release locks

                co_await update2Task;
                CODING_ERROR_ASSERT(update2Task.IsComplete() == true);

                co_await tx2->CommitAsync();
            }
            co_return;
        }

        ktl::Awaitable<void> CloseStore_WhileInfiniteTimeout_ShouldSucceed_Test()
        {
            KBuffer::SPtr key = ToBuffer(5);
            KBuffer::SPtr value = ToBuffer(6);

            {
                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
                co_await Store->AddAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None);
                co_await tx->CommitAsync();
            }

            {
                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx1 = CreateWriteTransaction();
                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx2 = CreateWriteTransaction();

                auto update1Task = Store->ConditionalUpdateAsync(*tx1->StoreTransactionSPtr, key, ToBuffer(8), Common::TimeSpan::FromTicks(-1), CancellationToken::None);
                auto update2Task = Store->ConditionalUpdateAsync(*tx2->StoreTransactionSPtr, key, ToBuffer(8), Common::TimeSpan::FromTicks(-1), CancellationToken::None);

                co_await update1Task;
                CODING_ERROR_ASSERT(update2Task.IsComplete() == false);

                // First update is not disposed, so locks are still held

                CloseAndReOpenStore(); // update2Task should expire

                try
                {
                    co_await update2Task;
                }
                catch (ktl::Exception const & e)
                {
                    CODING_ERROR_ASSERT(e.GetStatus() == SF_STATUS_TRANSACTION_ABORTED);
                }
            }

            co_return;
        }

        ktl::Awaitable<void> WriteStore_NotPrimary_ShouldFail_Test()
        {
            Replicator->SetWriteStatus(FABRIC_SERVICE_PARTITION_ACCESS_STATUS_NOT_PRIMARY);
            bool hasFailed = false;

            KBuffer::SPtr key = ToBuffer(5);
            KBuffer::SPtr value = ToBuffer(6);

            WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
            try
            {
                co_await Store->AddAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None);
            }
            catch (ktl::Exception & e)
            {
                hasFailed = true;
                CODING_ERROR_ASSERT(e.GetStatus() == SF_STATUS_NOT_PRIMARY);
            }

            CODING_ERROR_ASSERT(hasFailed);
            co_await tx->AbortAsync();

            co_await VerifyKeyDoesNotExistAsync(*Store, key);
            co_return;
        }

        ktl::Awaitable<void> ReadStore_NotReadable_ShouldFail_Test()
        {
            bool hasFailed = false;

            KBuffer::SPtr key = ToBuffer(5);
            KBuffer::SPtr value = ToBuffer(6);

            {
                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
                co_await Store->AddAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None);
                co_await tx->CommitAsync();
            }

            Replicator->SetReadStatus(FABRIC_SERVICE_PARTITION_ACCESS_STATUS_RECONFIGURATION_PENDING);

            WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
            try
            {
                co_await VerifyKeyExistsAsync(*Store, *tx->StoreTransactionSPtr, key, nullptr, value, EqualityFunction);
            }
            catch (ktl::Exception & e)
            {
                hasFailed = true;
                CODING_ERROR_ASSERT(e.GetStatus() == SF_STATUS_NOT_READABLE);
            }
            co_await tx->AbortAsync();
            CODING_ERROR_ASSERT(hasFailed);
            co_return;
        }

        ktl::Awaitable<void> Add_ConditionalGet_ShouldSucceed_Test()
        {
            KBuffer::SPtr key = ToBuffer(5);
            KBuffer::SPtr key2 = ToBuffer(6);
            KBuffer::SPtr value = ToBuffer(6);
            KBuffer::SPtr updatedValue = ToBuffer(8);
            LONG64 lsn = -1;

            // Add
            {
                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
                co_await Store->AddAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None);
                co_await Store->AddAsync(*tx->StoreTransactionSPtr, key2, value, DefaultTimeout, CancellationToken::None);
                lsn = co_await tx->CommitAsync();
            }

            // Get
            {
                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
                KeyValuePair<LONG64, KBuffer::SPtr> kvpair(-1, ToBuffer(0));
                KeyValuePair<LONG64, KBuffer::SPtr> kvpair2(-1, ToBuffer(0));
                co_await Store->ConditionalGetAsync(*tx->StoreTransactionSPtr, key, DefaultTimeout, kvpair, CancellationToken::None);
                CODING_ERROR_ASSERT(kvpair.Key == lsn);
                CODING_ERROR_ASSERT(*kvpair.Value == *value);
                co_await Store->ConditionalGetAsync(*tx->StoreTransactionSPtr, key2, DefaultTimeout, kvpair2, CancellationToken::None);
                CODING_ERROR_ASSERT(kvpair2.Key == lsn);
                CODING_ERROR_ASSERT(*kvpair2.Value == *value);
                tx->StoreTransactionSPtr->Unlock();
            }

            // Update
            {
                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
                bool update = co_await Store->ConditionalUpdateAsync(*tx->StoreTransactionSPtr, key, updatedValue, DefaultTimeout, CancellationToken::None, lsn);
                CODING_ERROR_ASSERT(update);
                lsn = co_await tx->CommitAsync();
            }

            // Get
            {
                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
                KeyValuePair<LONG64, KBuffer::SPtr> kvpair(-1, ToBuffer(0));
                co_await Store->ConditionalGetAsync(*tx->StoreTransactionSPtr, key, DefaultTimeout, kvpair, CancellationToken::None);
                CODING_ERROR_ASSERT(kvpair.Key == lsn);
                CODING_ERROR_ASSERT(*kvpair.Value == *updatedValue);
                tx->StoreTransactionSPtr->Unlock();
            }
            co_return;
        }

        ktl::Awaitable<void> Add_ConditionalUpdate_ShouldSucceed_Test()
        {
           KBuffer::SPtr key = ToBuffer(5);
           KBuffer::SPtr value = ToBuffer(6);
           LONG64 lsn = -1;
           KBuffer::SPtr updatedValue = ToBuffer(8);

           {
              WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
              co_await Store->AddAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None);
              lsn = co_await tx->CommitAsync();
           }

           {
              WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
              bool update = co_await Store->ConditionalUpdateAsync(*tx->StoreTransactionSPtr, key, updatedValue, DefaultTimeout, CancellationToken::None, 1);
              CODING_ERROR_ASSERT(update == false);
              co_await tx->AbortAsync();
           }

           co_await VerifyKeyExistsAsync(*Store, key, nullptr, value, EqualityFunction);

           {
              WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
              bool update = co_await Store->ConditionalUpdateAsync(*tx->StoreTransactionSPtr, key, updatedValue, DefaultTimeout, CancellationToken::None, lsn);
              CODING_ERROR_ASSERT(update);
              co_await tx->CommitAsync();
           }

           co_await VerifyKeyExistsAsync(*Store, key, nullptr, updatedValue, EqualityFunction);
            co_return;
        }

        ktl::Awaitable<void> Add_ConditionalDelete_ShouldSucceed_Test()
        {
           KBuffer::SPtr key = ToBuffer(5);
           KBuffer::SPtr value = ToBuffer(6);
           LONG64 lsn = -1;

           {
              WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
              co_await Store->AddAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None);
              lsn = co_await tx->CommitAsync();
           }

           {
              WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
              bool remove = co_await Store->ConditionalRemoveAsync(*tx->StoreTransactionSPtr, key, DefaultTimeout, CancellationToken::None, 1);
              CODING_ERROR_ASSERT(remove == false);
              co_await tx->AbortAsync();
           }

           co_await VerifyKeyExistsAsync(*Store, key, nullptr, value, EqualityFunction);

           {
              WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
              bool remove = co_await Store->ConditionalRemoveAsync(*tx->StoreTransactionSPtr, key, DefaultTimeout, CancellationToken::None, lsn);
              CODING_ERROR_ASSERT(remove);
              co_await tx->CommitAsync();
           }

           co_await VerifyKeyDoesNotExistAsync(*Store, key);
            co_return;
        }

        ktl::Awaitable<void> SnapshotRead_FromSnapshotContainer_MovedFromDifferentialState_Test()
        {
            KBuffer::SPtr key = ToBuffer(5);
            KBuffer::SPtr value = ToBuffer(6);

            // Add
            {
                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx1 = CreateWriteTransaction();
                co_await Store->AddAsync(*tx1->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None);
                co_await tx1->CommitAsync();
            }

            // Start the snapshot transactiion here
            WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx3 = CreateWriteTransaction();
            tx3->StoreTransactionSPtr->ReadIsolationLevel = StoreTransactionReadIsolationLevel::Snapshot;

            co_await VerifyKeyExistsAsync(*Store, *tx3->StoreTransactionSPtr, key, nullptr, value, EqualityFunction);

            // Update causes entries to previous version.
            {
                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx2 = CreateWriteTransaction();
                co_await Store->ConditionalUpdateAsync(*tx2->StoreTransactionSPtr, key, ToBuffer(7), DefaultTimeout, CancellationToken::None);
                co_await tx2->CommitAsync();
            }

            // Update again to move entries to snapshot container
            {
                WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx2 = CreateWriteTransaction();
                co_await Store->ConditionalUpdateAsync(*tx2->StoreTransactionSPtr, key, ToBuffer(8), DefaultTimeout, CancellationToken::None);
                co_await tx2->CommitAsync();
            }

            co_await VerifyKeyExistsAsync(*Store, key, nullptr, ToBuffer(8), EqualityFunction);

            co_await VerifyKeyExistsAsync(*Store, *tx3->StoreTransactionSPtr, key, nullptr, value, EqualityFunction);
            co_await tx3->AbortAsync();
            co_return;
        }

        ktl::Awaitable<void> SnapshotRead_FromConsolidatedState_Test()
        {
            ULONG32 count = 4;

            for (ULONG32 idx = 0; idx < count; idx++)
            {
                {
                    WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx1 = CreateWriteTransaction();
                    co_await Store->AddAsync(*tx1->StoreTransactionSPtr, ToBuffer(idx), ToBuffer(idx), DefaultTimeout, CancellationToken::None);
                    co_await tx1->CommitAsync();
                }
            }

            // Start the snapshot transactiion here
            WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
            tx->StoreTransactionSPtr->ReadIsolationLevel = StoreTransactionReadIsolationLevel::Snapshot;

            for (ULONG32 idx = 0; idx < count; idx++)
            {
                co_await VerifyKeyExistsAsync(*Store, *tx->StoreTransactionSPtr, ToBuffer(idx), nullptr, ToBuffer(idx), EqualityFunction);
            }

            co_await CheckpointAsync();

            // Update after checkpoKBuffer::SPtr.
            for (ULONG32 idx = 0; idx < count; idx++)
            {
                {
                    WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx1 = CreateWriteTransaction();
                    co_await Store->ConditionalUpdateAsync(*tx1->StoreTransactionSPtr, ToBuffer(idx), ToBuffer(idx + 10), DefaultTimeout, CancellationToken::None);
                    co_await tx1->CommitAsync();
                }
            }

            for (ULONG32 idx = 0; idx < count; idx++)
            {
                co_await VerifyKeyExistsAsync(*Store, ToBuffer(idx), nullptr, ToBuffer(idx + 10), EqualityFunction);
            }

            for (ULONG32 idx = 0; idx < count; idx++)
            {
                co_await VerifyKeyExistsAsync(*Store, *tx->StoreTransactionSPtr, ToBuffer(idx), nullptr, ToBuffer(idx), EqualityFunction);
            }

            co_await tx->AbortAsync();
            co_return;
        }

        ktl::Awaitable<void> SnapshotRead_FromSnapshotContainer_MovedFromDifferential_DuringConsolidation_Test()
        {
            ULONG32 count = 4;
            for (ULONG32 idx = 0; idx < count; idx++)
            {
                {
                    WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx1 = CreateWriteTransaction();
                    co_await Store->AddAsync(*tx1->StoreTransactionSPtr, ToBuffer(idx), ToBuffer(idx), DefaultTimeout, CancellationToken::None);
                    co_await tx1->CommitAsync();
                }
            }

            // Start the snapshot transactiion here
            WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
            tx->StoreTransactionSPtr->ReadIsolationLevel = StoreTransactionReadIsolationLevel::Snapshot;

            // Read to start snapping visibility lsn here.
            for (ULONG32 idx = 0; idx < count; idx++)
            {
                co_await VerifyKeyExistsAsync(*Store, *tx->StoreTransactionSPtr, ToBuffer(idx), nullptr, ToBuffer(idx), EqualityFunction);
            }

            for (ULONG32 idx = 0; idx < count; idx++)
            {
                {
                    WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx1 = CreateWriteTransaction();
                    co_await Store->ConditionalUpdateAsync(*tx1->StoreTransactionSPtr, ToBuffer(idx), ToBuffer(idx + 10), DefaultTimeout, CancellationToken::None);
                    co_await tx1->CommitAsync();
                }
            }

            // Read updated value to validate.
            for (ULONG32 idx = 0; idx < count; idx++)
            {
                co_await VerifyKeyExistsAsync(*Store, ToBuffer(idx), nullptr, ToBuffer(idx + 10), EqualityFunction);
            }

            co_await CheckpointAsync();

            for (ULONG32 idx = 0; idx < count; idx++)
            {
                {
                    WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx1 = CreateWriteTransaction();
                    co_await Store->ConditionalUpdateAsync(*tx1->StoreTransactionSPtr, ToBuffer(idx), ToBuffer(idx + 20), DefaultTimeout, CancellationToken::None);
                    co_await tx1->CommitAsync();
                }
            }

            // Read updated value to validate.
            for (ULONG32 idx = 0; idx < count; idx++)
            {
                co_await VerifyKeyExistsAsync(*Store, ToBuffer(idx), nullptr, ToBuffer(idx + 20), EqualityFunction);
            }

            for (ULONG32 idx = 0; idx < count; idx++)
            {
                co_await VerifyKeyExistsAsync(*Store, *tx->StoreTransactionSPtr, ToBuffer(idx), nullptr, ToBuffer(idx), EqualityFunction);
            }

            co_await tx->AbortAsync();
            co_return;
        }

        ktl::Awaitable<void> SnapshotRead_FromSnahotContainer_MovedFromConsolidatedState_Test()
        {
            ULONG32 count = 4;
            for (ULONG32 idx = 0; idx < count; idx++)
            {
                {
                    WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx1 = CreateWriteTransaction();
                    co_await Store->AddAsync(*tx1->StoreTransactionSPtr, ToBuffer(idx), ToBuffer(idx), DefaultTimeout, CancellationToken::None);
                    co_await tx1->CommitAsync();
                }
            }

            // Start the snapshot transactiion here
            WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
            tx->StoreTransactionSPtr->ReadIsolationLevel = StoreTransactionReadIsolationLevel::Snapshot;

            // Read to start snapping visibility lsn here.
            for (ULONG32 idx = 0; idx < count; idx++)
            {
                co_await VerifyKeyExistsAsync(*Store, *tx->StoreTransactionSPtr, ToBuffer(idx), nullptr, ToBuffer(idx), EqualityFunction);
            }

            co_await CheckpointAsync();

            for (ULONG32 idx = 0; idx < count; idx++)
            {
                {
                    WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx1 = CreateWriteTransaction();
                    co_await Store->ConditionalUpdateAsync(*tx1->StoreTransactionSPtr, ToBuffer(idx), ToBuffer(idx + 10), DefaultTimeout, CancellationToken::None);
                    co_await tx1->CommitAsync();
                }
            }

            co_await CheckpointAsync();

            // Read updated value to validate.
            for (ULONG32 idx = 0; idx < count; idx++)
            {
                co_await VerifyKeyExistsAsync(*Store, ToBuffer(idx), nullptr, ToBuffer(idx + 10), EqualityFunction);
            }

            for (ULONG32 idx = 0; idx < count; idx++)
            {
                co_await VerifyKeyExistsAsync(*Store, *tx->StoreTransactionSPtr, ToBuffer(idx), nullptr, ToBuffer(idx), EqualityFunction);
            }

            co_await tx->AbortAsync();
            co_return;
        }
    #pragma endregion
    };

    BOOST_FIXTURE_TEST_SUITE(StoreTestBuffer1ReplicaSuite, StoreTestBuffer1Replica)

        BOOST_AUTO_TEST_CASE(Add_SingleTransaction_ShouldSucceed)
    {
        SyncAwait(Add_SingleTransaction_ShouldSucceed_Test());
    }

    BOOST_AUTO_TEST_CASE(AddUpdate_SingleTransaction_ShouldSucceed)
    {
        SyncAwait(AddUpdate_SingleTransaction_ShouldSucceed_Test());
    }

    BOOST_AUTO_TEST_CASE(AddDelete_SingleTransaction_ShouldSucceed)
    {
        SyncAwait(AddDelete_SingleTransaction_ShouldSucceed_Test());
    }

    BOOST_AUTO_TEST_CASE(AddDeleteAdd_SingleTransaction_ShouldSucceed)
    {
        SyncAwait(AddDeleteAdd_SingleTransaction_ShouldSucceed_Test());
    }

    BOOST_AUTO_TEST_CASE(NoAdd_Delete_ShouldFail)
    {
        SyncAwait(NoAdd_Delete_ShouldFail_Test());
    }

    BOOST_AUTO_TEST_CASE(AddDeleteUpdate_SingleTransaction_ShouldFail)
    {
        SyncAwait(AddDeleteUpdate_SingleTransaction_ShouldFail_Test());
    }

    BOOST_AUTO_TEST_CASE(AddAdd_SingleTransaction_ShouldFail)
    {
        SyncAwait(AddAdd_SingleTransaction_ShouldFail_Test());
    }

    BOOST_AUTO_TEST_CASE(MultipleAdds_SingleTransaction_ShouldSucceed)
    {
        SyncAwait(MultipleAdds_SingleTransaction_ShouldSucceed_Test());
    }

    BOOST_AUTO_TEST_CASE(MultipleUpdates_SingleTransaction_ShouldSucceed)
    {
        SyncAwait(MultipleUpdates_SingleTransaction_ShouldSucceed_Test());
    }

    BOOST_AUTO_TEST_CASE(AddUpdate_DifferentTransactions_ShouldSucceed)
    {
        SyncAwait(AddUpdate_DifferentTransactions_ShouldSucceed_Test());
    }

    BOOST_AUTO_TEST_CASE(AddDelete_DifferentTransactions_ShouldSucceed)
    {
        SyncAwait(AddDelete_DifferentTransactions_ShouldSucceed_Test());
    }

    BOOST_AUTO_TEST_CASE(AddUpdateRead_DifferentTransactions_ShouldSucceed)
    {
        SyncAwait(AddUpdateRead_DifferentTransactions_ShouldSucceed_Test());
    }

    BOOST_AUTO_TEST_CASE(AddDeleteUpdate_DifferentTransactions_ShouldFail)
    {
        SyncAwait(AddDeleteUpdate_DifferentTransactions_ShouldFail_Test());
    }

    BOOST_AUTO_TEST_CASE(MultipleAdds_MultipleTransactions_ShouldSucceed)
    {
        SyncAwait(MultipleAdds_MultipleTransactions_ShouldSucceed_Test());
    }

    BOOST_AUTO_TEST_CASE(MultipleAddsUpdates_MultipleTransactions_ShouldSucceed)
    {
        SyncAwait(MultipleAddsUpdates_MultipleTransactions_ShouldSucceed_Test());
    }

    BOOST_AUTO_TEST_CASE(Add_Abort_ShouldSucceed)
    {
        SyncAwait(Add_Abort_ShouldSucceed_Test());
    }

    BOOST_AUTO_TEST_CASE(AddAdd_SameKeyOnConcurrentTransaction_ShouldTimeout)
    {
        SyncAwait(AddAdd_SameKeyOnConcurrentTransaction_ShouldTimeout_Test());
    }

    BOOST_AUTO_TEST_CASE(UpdateRead_SameKeyOnConcurrentTransaction_ShouldTimeout)
    {
        SyncAwait(UpdateRead_SameKeyOnConcurrentTransaction_ShouldTimeout_Test());
    }

    BOOST_AUTO_TEST_CASE(UpdateUpdate_SameKeyOnConcurrentTransaction_WithInfiniteTimeout_ShouldSucceed)
    {
        SyncAwait(UpdateUpdate_SameKeyOnConcurrentTransaction_WithInfiniteTimeout_ShouldSucceed_Test());
    }

    BOOST_AUTO_TEST_CASE(CloseStore_WhileInfiniteTimeout_ShouldSucceed)
    {
        SyncAwait(CloseStore_WhileInfiniteTimeout_ShouldSucceed_Test());
    }

    BOOST_AUTO_TEST_CASE(WriteStore_NotPrimary_ShouldFail)
    {
        SyncAwait(WriteStore_NotPrimary_ShouldFail_Test());
    }

    BOOST_AUTO_TEST_CASE(ReadStore_NotReadable_ShouldFail)
    {
        SyncAwait(ReadStore_NotReadable_ShouldFail_Test());
    }

    BOOST_AUTO_TEST_CASE(Add_ConditionalGet_ShouldSucceed)
    {
        SyncAwait(Add_ConditionalGet_ShouldSucceed_Test());
    }

    BOOST_AUTO_TEST_CASE(Add_ConditionalUpdate_ShouldSucceed)
    {
        SyncAwait(Add_ConditionalUpdate_ShouldSucceed_Test());
    }

    BOOST_AUTO_TEST_CASE(Add_ConditionalDelete_ShouldSucceed)
    {
        SyncAwait(Add_ConditionalDelete_ShouldSucceed_Test());
    }

    BOOST_AUTO_TEST_CASE(SnapshotRead_FromSnapshotContainer_MovedFromDifferentialState)
    {
        SyncAwait(SnapshotRead_FromSnapshotContainer_MovedFromDifferentialState_Test());
    }

    BOOST_AUTO_TEST_CASE(SnapshotRead_FromConsolidatedState)
    {
        SyncAwait(SnapshotRead_FromConsolidatedState_Test());
    }

    BOOST_AUTO_TEST_CASE(SnapshotRead_FromSnapshotContainer_MovedFromDifferential_DuringConsolidation)
    {
        SyncAwait(SnapshotRead_FromSnapshotContainer_MovedFromDifferential_DuringConsolidation_Test());
    }

    BOOST_AUTO_TEST_CASE(SnapshotRead_FromSnahotContainer_MovedFromConsolidatedState)
    {
        SyncAwait(SnapshotRead_FromSnahotContainer_MovedFromConsolidatedState_Test());
    }

    BOOST_AUTO_TEST_SUITE_END()
}
