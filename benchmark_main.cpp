#include "cache_warm.cpp"

auto main() -> int {
    try {
        int isConsistent = pmem::obj::pool<RootType>::check(PoolPath, PoolLayout);
        if (isConsistent > 0)
            Pool = pmem::obj::pool<RootType>::open(PoolPath, PoolLayout);
        else if (isConsistent < 0)
            Pool = pmem::obj::pool<RootType>::create(PoolPath, PoolLayout, POOL_SIZE);
        else {
            std::cout << "IsConsistent: " << isConsistent << std::endl;
            assert(false);
        }
        RootPtr = Pool.root();

        // params are name, prepare, benchmark
        // pick one the following

        ExperimentDataType{ "SystemCall_Read", 1, "NoWrapper", "", "ReadFromPersistenceBenchmark" }.Execute();
        ExperimentDataType{ "SystemCall_Warm_Read", 1, "NoWrapper", "WarmCachePrepare", "ReadFromPersistenceBenchmark" }.Execute();
        ExperimentDataType{ "SystemCall_Cool_Read", 1, "NoWrapper", "CoolCachePrepare", "ReadFromPersistenceBenchmark" }.Execute();
        ExperimentDataType{ "SystemCall_ComplexCool_Read", 1, "NoWrapper", "ComplexCoolCachePrepare", "ReadFromPersistenceBenchmark" }.Execute();

        ExperimentDataType{ "SystemCall_Write", 1, "NoWrapper", "", "WriteToPersistenceBenchmark" }.Execute();
        ExperimentDataType{ "SystemCall_Warm_Write", 1, "NoWrapper", "WarmCachePrepare", "WriteToPersistenceBenchmark" }.Execute();
        ExperimentDataType{ "SystemCall_Cool_Write", 1, "NoWrapper", "CoolCachePrepare", "WriteToPersistenceBenchmark" }.Execute();
        ExperimentDataType{ "SystemCall_ComplexCool_Write", 1, "NoWrapper", "ComplexCoolCachePrepare", "WriteToPersistenceBenchmark" }.Execute();

    } catch (const pmem::pool_error& e) {
        std::cout << e.what() << '\n';
        assert(false);
    } catch (const pmem::transaction_error& e) {
        std::cout << e.what() << '\n';
        assert(false);
    } catch (const std::bad_alloc& e) {
        std::cout << e.what() << '\n';
        assert(false);
    }
    return 0;
}
