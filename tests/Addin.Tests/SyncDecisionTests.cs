using RvtFileInfo.Store;
using Xunit;

namespace RvtFileInfo.Tests;

public class SyncDecisionTests
{
    [Fact]
    public void ImportsWhenFileIsNewer()
    {
        Assert.True(SyncDecision.ShouldImport("2026-02-02T00:00:00Z", "2026-01-01T00:00:00Z"));
    }

    [Fact]
    public void ModelWinsOnTie()
    {
        Assert.False(SyncDecision.ShouldImport("2026-01-01T00:00:00Z", "2026-01-01T00:00:00Z"));
    }

    [Fact]
    public void ImportsWhenModelHasNoTimestamp()
    {
        Assert.True(SyncDecision.ShouldImport("2026-01-01T00:00:00Z", ""));
        Assert.True(SyncDecision.ShouldImport("2026-01-01T00:00:00Z", null));
    }

    [Fact]
    public void SkipsWhenFileHasNoTimestamp()
    {
        Assert.False(SyncDecision.ShouldImport(null, "2026-01-01T00:00:00Z"));
        Assert.False(SyncDecision.ShouldImport("", "2026-01-01T00:00:00Z"));
    }
}
