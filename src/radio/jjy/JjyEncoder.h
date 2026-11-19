#pragma once

namespace JjyEncoder {
    using CarrierOnFn = void (*)();
    using CarrierOffFn = void (*)();

    void setCarrierCallbacks(CarrierOnFn on, CarrierOffFn off);

    // Called every 100 ms while JJY transmission is active.
    void onTicker();
}
