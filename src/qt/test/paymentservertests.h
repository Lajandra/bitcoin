// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_PAYMENTSERVERTESTS_H
#define BITCOIN_QT_TEST_PAYMENTSERVERTESTS_H

#include <qt/paymentserver.h>

#include <QObject>
#include <QTest>

namespace interfaces {
class Init;
} //namespace interfaces

class PaymentServerTests : public QObject
{
public:
    PaymentServerTests(interfaces::Init& init) : m_init(init) {}
    interfaces::Init& m_init;

    Q_OBJECT

private Q_SLOTS:
    void paymentServerTests();
};

// Dummy class to receive paymentserver signals.
// If SendCoinsRecipient was a proper QObject, then
// we could use QSignalSpy... but it's not.
class RecipientCatcher : public QObject
{
    Q_OBJECT

public Q_SLOTS:
    void getRecipient(const SendCoinsRecipient& r);

public:
    SendCoinsRecipient recipient;
};

#endif // BITCOIN_QT_TEST_PAYMENTSERVERTESTS_H
