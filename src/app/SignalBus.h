#pragma once

#include <QtCore/QObject>
#include "common/Types.h"

class SignalBus : public QObject {
    Q_OBJECT

public:
    explicit SignalBus(QObject* parent = nullptr) : QObject(parent) {}

signals:
    void modeChanged(Mode newMode);
    void styleChanged(const StyleConfig& style);
    void globalVisibilityChanged(bool visible);
};
