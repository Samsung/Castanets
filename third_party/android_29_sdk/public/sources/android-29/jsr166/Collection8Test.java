/*
 * Written by Doug Lea and Martin Buchholz with assistance from
 * members of JCP JSR-166 Expert Group and released to the public
 * domain, as explained at
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

package jsr166;

import static java.util.concurrent.TimeUnit.MILLISECONDS;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executors;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Future;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicLong;
import java.util.function.Consumer;

import junit.framework.Test;

/**
 * Contains tests applicable to all jdk8+ Collection implementations.
 * An extension of CollectionTest.
 */
// Android-changed: Made class abstract so it will be ignored by test runners.
public abstract class Collection8Test extends JSR166TestCase {
    final CollectionImplementation impl;

    /** Tests are parameterized by a Collection implementation. */
    Collection8Test(CollectionImplementation impl, String methodName) {
        super(methodName);
        this.impl = impl;
    }

    public static Test testSuite(CollectionImplementation impl) {
        return parameterizedTestSuite(Collection8Test.class,
                                      CollectionImplementation.class,
                                      impl);
    }

    /**
     * stream().forEach returns elements in the collection
     */
    public void testForEach() throws Throwable {
        final Collection c = impl.emptyCollection();
        final AtomicLong count = new AtomicLong(0L);
        final Object x = impl.makeElement(1);
        final Object y = impl.makeElement(2);
        final ArrayList found = new ArrayList();
        Consumer<Object> spy = (o) -> { found.add(o); };
        c.stream().forEach(spy);
        assertTrue(found.isEmpty());

        assertTrue(c.add(x));
        c.stream().forEach(spy);
        assertEquals(Collections.singletonList(x), found);
        found.clear();

        assertTrue(c.add(y));
        c.stream().forEach(spy);
        assertEquals(2, found.size());
        assertTrue(found.contains(x));
        assertTrue(found.contains(y));
        found.clear();

        c.clear();
        c.stream().forEach(spy);
        assertTrue(found.isEmpty());
    }

    public void testForEachConcurrentStressTest() throws Throwable {
        if (!impl.isConcurrent()) return;
        final Collection c = impl.emptyCollection();
        final long testDurationMillis = timeoutMillis();
        final AtomicBoolean done = new AtomicBoolean(false);
        final Object elt = impl.makeElement(1);
        final Future<?> f1, f2;
        final ExecutorService pool = Executors.newCachedThreadPool();
        try (PoolCleaner cleaner = cleaner(pool, done)) {
            final CountDownLatch threadsStarted = new CountDownLatch(2);
            Runnable checkElt = () -> {
                threadsStarted.countDown();
                while (!done.get())
                    c.stream().forEach((x) -> { assertSame(x, elt); }); };
            Runnable addRemove = () -> {
                threadsStarted.countDown();
                while (!done.get()) {
                    assertTrue(c.add(elt));
                    assertTrue(c.remove(elt));
                }};
            f1 = pool.submit(checkElt);
            f2 = pool.submit(addRemove);
            Thread.sleep(testDurationMillis);
        }
        assertNull(f1.get(0L, MILLISECONDS));
        assertNull(f2.get(0L, MILLISECONDS));
    }

    // public void testCollection8DebugFail() { fail(); }
}
