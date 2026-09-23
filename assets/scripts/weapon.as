class Weapon : MonoBehaviour
{
    [Range(1, 20), Tooltip("Выстрелов в сек")]
    int fireRate = 1;
    int ammo = 0;

    string effect = "Пыщ-Пыщ";

    void Fire()
    {
        print(effect);
        
        AudioComponent@ sound = GetOwner().GetAudio();

        if (sound != null)
        {
            sound.Play();
        }

        AnimatorComponent@ animator = GetOwner().GetAnimator();

        if (animator != null)
        {
            //animator.Play(1, true);
            animator.PlayQueue("attack", "idle", false);
        }
    }

    void Reload()
    {
        //print("Reload");
        AnimatorComponent@ animator = GetOwner().GetAnimator();

        if (animator != null)
        {
            //animator.Play(4, true);
            //animator.PlayQueue("reload", "idle", false);

            animator.Play("reload", true);
            ammo = 0;

            /*
            // Передаем ЛЯМБДУ (анонимную функцию) прямо в коллбэк!
            @animator.onClipFinished = function(const string &in clipName) {
                if (clipName == "reload") {
                    print("Лямбда: Перезарядка завершена!");
                }
            };
            */

            // Подписываемся на завершение анимации
            @animator.onClipFinished = OnClipFinishedCallback(this.handleAnimationEnd);
        }
    }

    void handleAnimationEnd(const string &in clipName)
    {
        if (clipName == "reload")
        {
            this.ammo = 30; // Выдаем патроны ТОЛЬКО когда анимация завершилась!

            AnimatorComponent@ animator = GetOwner().GetAnimator();
            animator.Play("idle", false); // Возвращаемся в idle

            @animator.onClipFinished = null; // Отписываемся
        }
    }

    void start()
    {
        print("Weapon spawned on entity: " + GetOwner().GetName());
    }

    void update(float dt)
    {
        if (Input::GetLeftMouseButtonClicked())
        {
            Fire();
        }

        if (Input::GetKeyDown(KeyCode::R))
        {
            Reload();
        }
    }
}