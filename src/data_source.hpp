#pragma once

class DataSource {
public:
    virtual ~DataSource() = default;
    virtual double next() = 0;
    virtual bool ready() const = 0;
};
