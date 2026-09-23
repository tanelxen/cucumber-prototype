class Weapon : MonoBehaviour
{
    [Range(1, 20), Tooltip("Выстрелов в сек")]
    int fireRate = 1;

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
            animator.Play(1, true);
        }
    }

    void Reload()
    {
        print("Reload");
        AnimatorComponent@ animator = GetOwner().GetAnimator();

        if (animator != null)
        {
            animator.Play(4, true);
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