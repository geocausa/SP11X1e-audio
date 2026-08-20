#include <linux/module.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/err.h>

static struct clk *wsa2x_clk;

static int __init sp11_wsa2x_vote_init(void)
{
    struct device_node *np;
    struct of_phandle_args spec = { 0 };
    int ret;

    np = of_find_compatible_node(NULL, NULL, "qcom,q6prm-lpass-clocks");
    if (!np)
        return -ENODEV;

    spec.np = np;
    spec.args_count = 2;
    spec.args[0] = 67; /* LPASS_CLK_ID_WSA_CORE_TX_2X_MCLK */
    spec.args[1] = 1;  /* LPASS_CLK_ATTRIBUTE_COUPLE_NO */

    wsa2x_clk = of_clk_get_from_provider(&spec);
    of_node_put(np);
    if (IS_ERR(wsa2x_clk)) {
        ret = PTR_ERR(wsa2x_clk);
        wsa2x_clk = NULL;
        pr_err("SP11_WSA2X_VOTE get failed=%d\n", ret);
        return ret;
    }

    ret = clk_set_rate(wsa2x_clk, 19200000);
    if (ret)
        goto err_put;
    ret = clk_prepare_enable(wsa2x_clk);
    if (ret)
        goto err_put;

    pr_info("SP11_WSA2X_VOTE enabled clk-id=0x315 rate=%lu\n", clk_get_rate(wsa2x_clk));
    return 0;
err_put:
    clk_put(wsa2x_clk);
    wsa2x_clk = NULL;
    return ret;
}

static void __exit sp11_wsa2x_vote_exit(void)
{
    if (wsa2x_clk) {
        clk_disable_unprepare(wsa2x_clk);
        clk_put(wsa2x_clk);
        wsa2x_clk = NULL;
    }
    pr_info("SP11_WSA2X_VOTE disabled clk-id=0x315\n");
}
module_init(sp11_wsa2x_vote_init);
module_exit(sp11_wsa2x_vote_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SP11 one-shot WSA_CORE_TX_2X_MCLK native-Windows parity vote");
