#include <linux/module.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/err.h>

static struct clk *rxcoretx_clk;

static int __init sp11_rxcoretx_vote_init(void)
{
    struct device_node *np;
    struct of_phandle_args spec = { 0 };
    int ret;

    np = of_find_compatible_node(NULL, NULL, "qcom,q6prm-lpass-clocks");
    if (!np) {
        pr_err("SP11_RXCORETX_VOTE provider node not found\n");
        return -ENODEV;
    }

    spec.np = np;
    spec.args_count = 2;
    spec.args[0] = 64; /* LPASS_CLK_ID_RX_CORE_TX_MCLK */
    spec.args[1] = 1;  /* LPASS_CLK_ATTRIBUTE_COUPLE_NO */

    rxcoretx_clk = of_clk_get_from_provider(&spec);
    of_node_put(np);
    if (IS_ERR(rxcoretx_clk)) {
        ret = PTR_ERR(rxcoretx_clk);
        pr_err("SP11_RXCORETX_VOTE get failed=%d\n", ret);
        rxcoretx_clk = NULL;
        return ret;
    }

    ret = clk_set_rate(rxcoretx_clk, 19200000);
    if (ret) {
        pr_err("SP11_RXCORETX_VOTE set_rate failed=%d\n", ret);
        goto err_put;
    }

    ret = clk_prepare_enable(rxcoretx_clk);
    if (ret) {
        pr_err("SP11_RXCORETX_VOTE enable failed=%d\n", ret);
        goto err_put;
    }

    pr_info("SP11_RXCORETX_VOTE enabled clk-id=0x312 rate=%lu\n", clk_get_rate(rxcoretx_clk));
    return 0;

err_put:
    clk_put(rxcoretx_clk);
    rxcoretx_clk = NULL;
    return ret;
}

static void __exit sp11_rxcoretx_vote_exit(void)
{
    if (rxcoretx_clk) {
        clk_disable_unprepare(rxcoretx_clk);
        clk_put(rxcoretx_clk);
        rxcoretx_clk = NULL;
    }
    pr_info("SP11_RXCORETX_VOTE disabled clk-id=0x312\n");
}

module_init(sp11_rxcoretx_vote_init);
module_exit(sp11_rxcoretx_vote_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SP11 one-shot RX_CORE_TX_MCLK parity vote");
